#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "chemicalite.h"
#include "bitstring.h"
#include "rdtree.h"

/* 
** This index data structure is *heavily* copied from the SQLite's
** r-tree and r*-tree implementation, with the necessary variation on the 
** algorithms which are required to turn it into an rd-tree.
*/

/*
** Database Format of RD-Tree Tables
** ---------------------------------
**
** The data structure for a single virtual rd-tree table is stored in three 
** native SQLite tables declared as follows. In each case, the '%' character
** in the table name is replaced with the user-supplied name of the rd-tree
** table.
**
**   CREATE TABLE %_node(nodeno INTEGER PRIMARY KEY, data BLOB)
**   CREATE TABLE %_parent(nodeno INTEGER PRIMARY KEY, parentnode INTEGER)
**   CREATE TABLE %_rowid(rowid INTEGER PRIMARY KEY, nodeno INTEGER)
**
** The data for each node of the rd-tree structure is stored in the %_node
** table. For each node that is not the root node of the r-tree, there is
** an entry in the %_parent table associating the node with its parent.
** And for each row of data in the table, there is an entry in the %_rowid
** table that maps from the entries rowid to the id of the node that it
** is stored on.
**
** The root node of an rd-tree always exists, even if the rd-tree table is
** empty. The nodeno of the root node is always 1. All other nodes in the
** table must be the same size as the root node. The content of each node
** is formatted as follows:
**
**   1. If the node is the root node (node 1), then the first 2 bytes
**      of the node contain the tree depth as a big-endian integer.
**      For non-root nodes, the first 2 bytes are left unused.
**
**   2. The next 2 bytes contain the number of entries currently 
**      stored in the node.
**
**   3. The remainder of the node contains the node entries. Each entry
**      consists of a single 64-bits integer followed by a binary fingerprint. 
**      For leaf nodes the integer is the rowid of a record. For internal
**      nodes it is the node number of a child page.
*/

typedef sqlite3_int64 i64;

typedef struct RDtree RDtree;
typedef struct RDtreeNode RDtreeNode;
typedef struct RDtreeItem RDtreeItem;

/*
** Functions to deserialize a 16 bit integer, 32 bit real number and
** 64 bit integer. The deserialized value is returned.
*/

static int readInt16(u8 *p)
{
  return (p[0] << 8) + p[1];
}

static i64 readInt64(u8 *p)
{
  return ( (((i64)p[0]) << 56) + 
	   (((i64)p[1]) << 48) + 
	   (((i64)p[2]) << 40) + 
	   (((i64)p[3]) << 32) + 
	   (((i64)p[4]) << 24) + 
	   (((i64)p[5]) << 16) + 
	   (((i64)p[6]) <<  8) + 
	   (((i64)p[7]) <<  0) );
}

/*
** Functions to serialize a 16 bit integer, 32 bit real number and
** 64 bit integer. The value returned is the number of bytes written
** to the argument buffer (always 2, 4 and 8 respectively).
*/
static int writeInt16(u8 *p, int i)
{
  p[0] = (i>> 8) & 0xFF;
  p[1] = (i>> 0) & 0xFF;
  return 2;
}

static int writeInt64(u8 *p, i64 i)
{
  p[0] = (i>>56) & 0xFF;
  p[1] = (i>>48) & 0xFF;
  p[2] = (i>>40) & 0xFF;
  p[3] = (i>>32) & 0xFF;
  p[4] = (i>>24) & 0xFF;
  p[5] = (i>>16) & 0xFF;
  p[6] = (i>> 8) & 0xFF;
  p[7] = (i>> 0) & 0xFF;
  return 8;
}

/* Size of hash table RDtree.aHash. This hash table is not expected to
** ever contain very many entries, so a fixed number of buckets is 
** used.
*/
#define HASHSIZE 128

/* 
** An rd-tree virtual-table object.
*/
struct RDtree {
  sqlite3_vtab base;
  sqlite3 *db;                 /* Host database connection */
  int iBfpSize;                /* Size (bytes) of the binary fingerprint */
  int nBytesPerItem;           /* Bytes consumed per item */
  int iNodeSize;               /* Size (bytes) of each node in the node table */
  int iDepth;                  /* Current depth of the rd-tree structure */
  char *zDb;                   /* Name of database containing rd-tree table */
  char *zName;                 /* Name of rd-tree table */ 
  RDtreeNode *aHash[HASHSIZE]; /* Hash table of in-memory nodes. */ 
  int nBusy;                   /* Current number of users of this structure */

  /* List of nodes removed during a CondenseTree operation. List is
  ** linked together via the pointer normally used for hash chains -
  ** RDtreeNode.pNext. RDtreeNode.iNode stores the depth of the sub-tree 
  ** headed by the node (leaf nodes have RDtreeNode.iNode==0).
  */
  RDtreeNode *pDeleted;
  int iReinsertHeight;  /* Height of sub-trees Reinsert() has run on DELME? */

  /* Statements to read/write/delete a record from xxx_node */
  sqlite3_stmt *pReadNode;
  sqlite3_stmt *pWriteNode;
  sqlite3_stmt *pDeleteNode;

  /* Statements to read/write/delete a record from xxx_rowid */
  sqlite3_stmt *pReadRowid;
  sqlite3_stmt *pWriteRowid;
  sqlite3_stmt *pDeleteRowid;

  /* Statements to read/write/delete a record from xxx_parent */
  sqlite3_stmt *pReadParent;
  sqlite3_stmt *pWriteParent;
  sqlite3_stmt *pDeleteParent;
};

/*
** The minimum number of cells allowed for a node is a third of the 
** maximum. In Gutman's notation:
**
**     m = M/3
**
** If an RD-tree "Reinsert" operation is required, the same number of
** cells are removed from the overfull node and reinserted into the tree.
*/
#define RDTREE_MINITEMS(p) ((((p)->iNodeSize-4)/(p)->nBytesPerItem)/3)
#define RDTREE_REINSERT(p) RDTREE_MINITEMS(p)
#define RTREE_MAXITEMS 51

/*
** The smallest possible node-size is (512-64)==448 bytes. And the largest
** supported cell size is 48 bytes (8 byte rowid + ten 4 byte coordinates).
** Therefore all non-root nodes must contain at least 3 entries. Since 
** 3^40 is greater than 2^64, an rd-tree structure always has a depth of
** 40 or less.
*/
/* FIXME FIXME FIXME */
#define RDTREE_MAX_DEPTH 40

/* 
** An rd-tree structure node.
*/
struct RDtreeNode {
  RDtreeNode *pParent; /* Parent node in the tree */
  i64 iNode;
  int nRef;
  int isDirty;
  u8 *zData;
  RDtreeNode *pNext;   /* Next node in this hash chain */
};
#define NITEM(pNode) readInt16(&(pNode)->zData[2])

/* 
** Structure to store a deserialized rd-tree record.
*/
struct RDtreeItem {
  i64 iRowid;
  u8 aBfp[MAX_BITSTRING_SIZE];
};
#define RDTREE_MAXITEMS 8

static int itemWeight(RDtree *pRDtree, RDtreeItem *pItem)
{
  /* FIXME FIXME FIXME */
  return 0;
}

/*
** Store the union of items p1 and p2 in p1.
*/
static void itemUnion(RDtree *pRDtree, RDtreeItem *p1, RDtreeItem *p2)
{
  /* FIXME FIXME FIXME */
}

/*
** Return true if item p2 is a subset of item p1. False otherwise.
*/
static int itemContains(RDtree *pRDtree, RDtreeItem *p1, RDtreeItem *p2)
{
  /* FIXME FIXME FIXME */
  return 0;
}

/*
** Return the amount item pBase would grow by if it were unioned with pAdded.
*/
static int itemGrowth(RDtree *pRDtree, RDtreeItem *pBase, RDtreeItem *pAdded)
{
  /* FIXME FIXME FIXME */
  return 0;
}

/*
** Increment the reference count of node p.
*/
static void nodeReference(RDtreeNode *p)
{
  if (p) { /* FIXME (assert p?) */
    p->nRef++; 
  }
}

/*
** Clear the content of node p (set all bytes to 0x00).
*/
static void nodeZero(RDtree *pRDtree, RDtreeNode *p)
{
  /* FIXME (assert p?) */
  memset(&p->zData[2], 0, pRDtree->iNodeSize-2);
  p->isDirty = 1;
}

/*
** Given a node number iNode, return the corresponding key to use
** in the RDtree.aHash table.
*/
static int nodeHash(i64 iNode)
{
  return ( (iNode>>56) ^ (iNode>>48) ^ (iNode>>40) ^ (iNode>>32) ^ 
	   (iNode>>24) ^ (iNode>>16) ^ (iNode>> 8) ^ (iNode>> 0)
	   ) % HASHSIZE;
}

/*
** Search the node hash table for node iNode. If found, return a pointer
** to it. Otherwise, return 0.
*/
static RDtreeNode *nodeHashLookup(RDtree *pRDtree, i64 iNode)
{
  RDtreeNode *p = pRDtree->aHash[nodeHash(iNode)]; 
  while (p && p->iNode != iNode) { 
    p = p->pNext;
  }
  return p;
}

/*
** Add node pNode to the node hash table.
*/
static void nodeHashInsert(RDtree *pRDtree, RDtreeNode *pNode)
{
  assert( pNode->pNext==0 ); /* not already cached */
  int iHash = nodeHash(pNode->iNode);
  pNode->pNext = pRDtree->aHash[iHash];
  pRDtree->aHash[iHash] = pNode;
}

/*
** Remove node pNode from the node hash table.
*/
static void nodeHashDelete(RDtree *pRDtree, RDtreeNode *pNode)
{
  if (pNode->iNode != 0) {
    RDtreeNode **pp = &pRDtree->aHash[nodeHash(pNode->iNode)];
    while ((*pp) != pNode) {
      pp = &(*pp)->pNext; 
      assert(*pp); 
    }
    *pp = pNode->pNext;
    pNode->pNext = 0;
  }
}

/*
** Allocate and return new rd-tree node. Initially, (RDtreeNode.iNode==0),
** indicating that node has not yet been assigned a node number. It is
** assigned a node number when nodeWrite() is called to write the
** node contents out to the database.
*/
static RDtreeNode *nodeNew(RDtree *pRDtree, RDtreeNode *pParent)
{
  RDtreeNode *pNode;
  pNode = (RDtreeNode *)sqlite3_malloc(sizeof(RDtreeNode) + pRDtree->iNodeSize);
  if( pNode ){
    memset(pNode, 0, sizeof(RDtreeNode) + pRDtree->iNodeSize);
    pNode->zData = (u8 *)&pNode[1];
    pNode->nRef = 1;
    pNode->pParent = pParent;
    pNode->isDirty = 1;
    nodeReference(pParent);
  }
  return pNode;
}

/*
** Obtain a reference to an rd-tree node.
*/
static int nodeAcquire(RDtree *pRDtree,     /* R-tree structure */
		       i64 iNode,           /* Node number to load */
		       RDtreeNode *pParent, /* Either the parent node or NULL */
		       RDtreeNode **ppNode) /* OUT: Acquired node */
{
  int rc;
  int rc2 = SQLITE_OK;
  RDtreeNode *pNode;

  /* Check if the requested node is already in the hash table. If so,
  ** increase its reference count and return it.
  */
  if ((pNode = nodeHashLookup(pRDtree, iNode))) {
    assert( !pParent || !pNode->pParent || pNode->pParent == pParent );
    if (pParent && !pNode->pParent) {
      nodeReference(pParent);
      pNode->pParent = pParent;
    }
    nodeReference(pNode);
    *ppNode = pNode;
    return SQLITE_OK;
  }

  sqlite3_bind_int64(pRDtree->pReadNode, 1, iNode);
  rc = sqlite3_step(pRDtree->pReadNode);

  if (rc == SQLITE_ROW) {
    const u8 *zBlob = sqlite3_column_blob(pRDtree->pReadNode, 0);
    if (pRDtree->iNodeSize == sqlite3_column_bytes(pRDtree->pReadNode, 0)) {
      pNode =
	(RDtreeNode *)sqlite3_malloc(sizeof(RDtreeNode) + pRDtree->iNodeSize);
      if (!pNode) {
        rc2 = SQLITE_NOMEM;
      }
      else{
        pNode->pParent = pParent;
        pNode->zData = (u8 *)&pNode[1];
        pNode->nRef = 1;
        pNode->iNode = iNode;
        pNode->isDirty = 0;
        pNode->pNext = 0;
        memcpy(pNode->zData, zBlob, pRDtree->iNodeSize);
        nodeReference(pParent);
      }
    }
  }

  rc = sqlite3_reset(pRDtree->pReadNode);
  if (rc == SQLITE_OK) {
    rc = rc2;
  }

  /* If the root node was just loaded, set pRDtree->iDepth to the height
  ** of the rd-tree structure. A height of zero means all data is stored on
  ** the root node. A height of one means the children of the root node
  ** are the leaves, and so on. If the depth as specified on the root node
  ** is greater than RDTREE_MAX_DEPTH, the rd-tree structure must be corrupt.
  */
  if (pNode && iNode == 1) {
    pRDtree->iDepth = readInt16(pNode->zData);
    if (pRDtree->iDepth > RDTREE_MAX_DEPTH) {
      rc = SQLITE_CORRUPT_VTAB;
    }
  }

  /* If no error has occurred so far, check if the "number of entries"
  ** field on the node is too large. If so, set the return code to 
  ** SQLITE_CORRUPT_VTAB.
  */
  if (pNode && rc == SQLITE_OK) {
    if (NITEM(pNode) > ((pRDtree->iNodeSize-4)/pRDtree->nBytesPerItem)) {
      rc = SQLITE_CORRUPT_VTAB;
    }
  }
  
  if (rc == SQLITE_OK) {
    if (pNode != 0) {
      nodeHashInsert(pRDtree, pNode);
    }
    else {
      rc = SQLITE_CORRUPT_VTAB;
    }
    *ppNode = pNode;
  }
  else {
    sqlite3_free(pNode);
    *ppNode = 0;
  }

  return rc;
}

/*
** Overwrite item iItem of node pNode with the contents of pItem.
*/
static void nodeOverwriteItem(RDtree *pRDtree, RDtreeNode *pNode,  
			      RDtreeItem *pItem, int iItem) {
  int ii;
  u8 *p = &pNode->zData[4 + pRDtree->nBytesPerItem*iItem];
  p += writeInt64(p, pItem->iRowid);
  memcpy(p, pItem->aBfp, pRDtree->iBfpSize);
  pNode->isDirty = 1;
}

/*
** Remove the item with index iItem from node pNode.
*/
static void nodeDeleteItem(RDtree *pRDtree, RDtreeNode *pNode, int iItem)
{
  u8 *pDst = &pNode->zData[4 + pRDtree->nBytesPerItem*iItem];
  u8 *pSrc = &pDst[pRDtree->nBytesPerItem];
  int nByte = (NITEM(pNode) - iItem - 1) * pRDtree->nBytesPerItem;
  memmove(pDst, pSrc, nByte);
  writeInt16(&pNode->zData[2], NITEM(pNode)-1);
  pNode->isDirty = 1;
}

/*
** Insert the contents of item pItem into node pNode. If the insert
** is successful, return SQLITE_OK.
**
** If there is not enough free space in pNode, return SQLITE_FULL.
*/
static int nodeInsertItem(RDtree *pRDtree, RDtreeNode *pNode, RDtreeItem *pItem)
{
  int nItem;    /* Current number of items in pNode */
  int nMaxItem; /* Maximum number of items for pNode */

  nMaxItem = (pRDtree->iNodeSize - 4)/pRDtree->nBytesPerItem;
  nItem = NITEM(pNode);

  assert(nItem <= nMaxItem);

  if (nItem < nMaxItem) {
    nodeOverwriteItem(pRDtree, pNode, pItem, nItem);
    writeInt16(&pNode->zData[2], nItem+1);
    pNode->isDirty = 1;
  }

  return (nItem == nMaxItem);
}

/*
** If the node is dirty, write it out to the database.
*/
static int nodeWrite(RDtree *pRDtree, RDtreeNode *pNode)
{
  int rc = SQLITE_OK;
  if (pNode->isDirty) {
    sqlite3_stmt *p = pRDtree->pWriteNode;
    if (pNode->iNode) {
      sqlite3_bind_int64(p, 1, pNode->iNode);
    }
    else {
      sqlite3_bind_null(p, 1);
    }
    sqlite3_bind_blob(p, 2, pNode->zData, pRDtree->iNodeSize, SQLITE_STATIC);
    sqlite3_step(p);
    pNode->isDirty = 0;
    rc = sqlite3_reset(p);
    if (pNode->iNode == 0 && rc == SQLITE_OK) {
      pNode->iNode = sqlite3_last_insert_rowid(pRDtree->db);
      nodeHashInsert(pRDtree, pNode);
    }
  }
  return rc;
}

/*
** Release a reference to a node. If the node is dirty and the reference
** count drops to zero, the node data is written to the database.
*/
static int nodeRelease(RDtree *pRDtree, RDtreeNode *pNode)
{
  int rc = SQLITE_OK;
  if (pNode) {
    assert(pNode->nRef > 0);
    pNode->nRef--;
    if (pNode->nRef == 0) {
      if (pNode->iNode == 1) {
        pRDtree->iDepth = -1;
      }
      if (pNode->pParent) {
        rc = nodeRelease(pRDtree, pNode->pParent);
      }
      if (rc == SQLITE_OK) {
        rc = nodeWrite(pRDtree, pNode);
      }
      nodeHashDelete(pRDtree, pNode);
      sqlite3_free(pNode);
    }
  }
  return rc;
}

/*
** Return the 64-bit integer value associated with item iItem of
** node pNode. If pNode is a leaf node, this is a rowid. If it is
** an internal node, then the 64-bit integer is a child page number.
*/
static i64 nodeGetRowid(RDtree *pRDtree, RDtreeNode *pNode, int iItem)
{
  assert(iItem < NITEM(pNode));
  return readInt64(&pNode->zData[4 + pRDtree->nBytesPerItem*iItem]);
}

/*
** Return pointer to the binary fingerprint associated with item iItem of
** node pNode. If pNode is a leaf node, this is a virtual table element.
** If it is an internal node, then the binary fingerprint defines the 
** bounds of a child node
*/
static u8 *nodeGetBfp(RDtree *pRDtree, RDtreeNode *pNode, int iItem)
{
  assert(iItem < NITEM(pNode));
  return &pNode->zData[4 + pRDtree->nBytesPerItem*iItem + 8];
}

/*
** Deserialize item iItem of node pNode. Populate the structure pointed
** to by pItem with the results.
*/
static void nodeGetItem(RDtree *pRDtree, RDtreeNode *pNode, 
			int iItem, RDtreeItem *pItem)
{
  i64 iRowid = nodeGetRowid(pRDtree, pNode, iItem);
  u8 *pBfp = nodeGetBfp(pRDtree, pNode, iItem);
  pItem->iRowid = iRowid;
  memcpy(pItem->aBfp, pBfp, pRDtree->iBfpSize);
}

/* Forward declaration for the function that does the work of
** the virtual table module xCreate() and xConnect() methods.
*/
static int rdtreeInit(sqlite3 *db, void *pAux, 
		      int argc, const char *const*argv, 
		      sqlite3_vtab **ppVtab, char **pzErr, int isCreate);

/* 
** RDtree virtual table module xCreate method.
*/
static int rdtreeCreate(sqlite3 *db, void *pAux,
			int argc, const char *const*argv,
			sqlite3_vtab **ppVtab,
			char **pzErr)
{
  return rdtreeInit(db, pAux, argc, argv, ppVtab, pzErr, 1);
}

/* 
** RDtree virtual table module xConnect method.
*/
static int rdtreeConnect(sqlite3 *db, void *pAux,
			 int argc, const char *const*argv,
			 sqlite3_vtab **ppVtab,
			 char **pzErr)
{
  return rdtreeInit(db, pAux, argc, argv, ppVtab, pzErr, 0);
}

/*
** Increment the rd-tree reference count.
*/
static void rdtreeReference(RDtree *pRDtree)
{
  pRDtree->nBusy++;
}

/*
** Decrement the rd-tree reference count. When the reference count reaches
** zero the structure is deleted.
*/
static void rdtreeRelease(RDtree *pRDtree)
{
  pRDtree->nBusy--;
  if (pRDtree->nBusy == 0) {
    sqlite3_finalize(pRDtree->pReadNode);
    sqlite3_finalize(pRDtree->pWriteNode);
    sqlite3_finalize(pRDtree->pDeleteNode);
    sqlite3_finalize(pRDtree->pReadRowid);
    sqlite3_finalize(pRDtree->pWriteRowid);
    sqlite3_finalize(pRDtree->pDeleteRowid);
    sqlite3_finalize(pRDtree->pReadParent);
    sqlite3_finalize(pRDtree->pWriteParent);
    sqlite3_finalize(pRDtree->pDeleteParent);
    sqlite3_free(pRDtree);
  }
}

/* 
** RDtree virtual table module xDisconnect method.
*/
static int rdtreeDisconnect(sqlite3_vtab *pVtab)
{
  rdtreeRelease((RDtree *)pVtab);
  return SQLITE_OK;
}

/* 
** RDtree virtual table module xDestroy method.
*/
static int rdtreeDestroy(sqlite3_vtab *pVtab)
{
  RDtree *pRDtree = (RDtree *)pVtab;
  int rc = SQLITE_OK;

  char *zCreate = sqlite3_mprintf("DROP TABLE '%q'.'%q_node';"
				  "DROP TABLE '%q'.'%q_rowid';"
				  "DROP TABLE '%q'.'%q_parent';",
				  pRDtree->zDb, pRDtree->zName, 
				  pRDtree->zDb, pRDtree->zName,
				  pRDtree->zDb, pRDtree->zName);

  if (!zCreate) {
    rc = SQLITE_NOMEM;
  }
  else{
    rc = sqlite3_exec(pRDtree->db, zCreate, 0, 0, 0);
    sqlite3_free(zCreate);
  }

  if (rc == SQLITE_OK) {
    rdtreeRelease(pRDtree);
  }

  return rc;
}

/*
** One of the items in node pNode is guaranteed to have a 64-bit 
** integer value equal to iRowid. Return the index of this item.
*/
static int nodeRowidIndex(RDtree *pRDtree, RDtreeNode *pNode, i64 iRowid,
			  int *piIndex)
{
  int ii;
  int nItem = NITEM(pNode);
  for (ii = 0; ii < nItem; ii++) {
    if (nodeGetRowid(pRDtree, pNode, ii) == iRowid) {
      *piIndex = ii;
      return SQLITE_OK;
    }
  }
  return SQLITE_CORRUPT_VTAB;
}

/*
** Return the index of the parent's item containing a pointer to node pNode.
** If pNode is the root node, return -1.
*/
static int nodeParentIndex(RDtree *pRDtree, RDtreeNode *pNode, int *piIndex)
{
  RDtreeNode *pParent = pNode->pParent;
  if (pParent) {
    return nodeRowidIndex(pRDtree, pParent, pNode->iNode, piIndex);
  }
  *piIndex = -1;
  return SQLITE_OK;
}

/* 
** Use nodeAcquire() to obtain the leaf node containing the record with 
** rowid iRowid. If successful, set *ppLeaf to point to the node and
** return SQLITE_OK. If there is no such record in the table, set
** *ppLeaf to 0 and return SQLITE_OK. If an error occurs, set *ppLeaf
** to zero and return an SQLite error code.
*/
static int findLeafNode(RDtree *pRDtree, i64 iRowid, RDtreeNode **ppLeaf)
{
  int rc = SQLITE_OK;
  *ppLeaf = 0;
  sqlite3_bind_int64(pRDtree->pReadRowid, 1, iRowid);
  if (sqlite3_step(pRDtree->pReadRowid) == SQLITE_ROW) {
    i64 iNode = sqlite3_column_int64(pRDtree->pReadRowid, 0);
    rc = nodeAcquire(pRDtree, iNode, 0, ppLeaf);
  }
  int rc2 = sqlite3_reset(pRDtree->pReadRowid);
  if (rc == SQLITE_OK) {
    rc = rc2;
  }
  return rc;
}

/*
** This function implements the chooseLeaf algorithm from Gutman[84].
** ChooseSubTree in r*tree terminology.
*/
static int chooseLeaf(RDtree *pRDtree,
		      RDtreeItem *pItem, /* Item to insert into rdtree */
		      int iHeight, /* Height of sub-tree rooted at pItem */
		      RDtreeNode **ppLeaf /* OUT: Selected leaf page */
		      )
{
  int rc;
  int ii;
  RDtreeNode *pNode;
  rc = nodeAcquire(pRDtree, 1, 0, &pNode);

  for (ii = 0; rc == SQLITE_OK && ii < (pRDtree->iDepth - iHeight); ii++) {

    /* FIXME FIXME FIXME */
    /* Select the child node which will be enlarged the least if pItem
    ** is inserted into it. Resolve ties by choosing the entry with
    ** the smallest area.
    */
  }

  *ppLeaf = pNode;
  return rc;
}

/*
** An item with the same content as pItem has just been inserted into
** the node pNode. This function updates the bounds in
** all ancestor elements.
*/
static int adjustTree(RDtree *pRDtree, RDtreeNode *pNode, RDtreeItem *pItem)
{
  RDtreeNode *p = pNode;
  while (p->pParent) {
    RDtreeNode *pParent = p->pParent;
    RDtreeItem item;
    int iItem;

    if (nodeParentIndex(pRDtree, p, &iItem)) {
      return SQLITE_CORRUPT_VTAB;
    }

    nodeGetItem(pRDtree, pParent, iItem, &item);
    if (!itemContains(pRDtree, &item, pItem) ){
      itemUnion(pRDtree, &item, pItem);
      nodeOverwriteItem(pRDtree, pParent, &item, iItem);
    }
 
    p = pParent;
  }
  return SQLITE_OK;
}

/*
** Write mapping (iRowid->iNode) to the <rdtree>_rowid table.
*/
static int rowidWrite(RDtree *pRDtree, 
		      sqlite3_int64 iRowid, sqlite3_int64 iNode)
{
  sqlite3_bind_int64(pRDtree->pWriteRowid, 1, iRowid);
  sqlite3_bind_int64(pRDtree->pWriteRowid, 2, iNode);
  sqlite3_step(pRDtree->pWriteRowid);
  return sqlite3_reset(pRDtree->pWriteRowid);
}

/*
** Write mapping (iNode->iPar) to the <rdtree>_parent table.
*/
static int parentWrite(RDtree *pRDtree, 
		       sqlite3_int64 iNode, sqlite3_int64 iPar)
{
  sqlite3_bind_int64(pRDtree->pWriteParent, 1, iNode);
  sqlite3_bind_int64(pRDtree->pWriteParent, 2, iPar);
  sqlite3_step(pRDtree->pWriteParent);
  return sqlite3_reset(pRDtree->pWriteParent);
}

/*
** Quadratic variant.
*/
static RDtreeItem *pickNext(RDtree *pRDtree,
			    RDtreeItem *aItem, int nItem, 
			    RDtreeItem *pLeftBounds, RDtreeItem *pRightBounds,
			    int *aiUsed)
{

  int iSelect = -1;
  int iMaxDiff;
  int ii;
  for(ii = 0; ii < nItem; ii++){
    if( aiUsed[ii]==0 ){
      int left = itemGrowth(pRDtree, pLeftBounds, &aItem[ii]);
      int right = itemGrowth(pRDtree, pRightBounds, &aItem[ii]);
      int diff = abs(right-left);
      if (iSelect < 0 || diff > iMaxDiff) {
        iMaxDiff = diff;
        iSelect = ii;
      }
    }
  }
  aiUsed[iSelect] = 1;
  return &aItem[iSelect];
}

/*
** Quadratic variant.
*/
static void pickSeeds(RDtree *pRDtree, RDtreeItem *aItem, int nItem, 
		      int *piLeftSeed, int *piRightSeed)
{
  int ii;
  int jj;

  int iLeftSeed = 0;
  int iRightSeed = 1;
  int iMaxWaste = 0.0;

  for (ii = 0; ii < nItem; ii++) {
    for (jj = ii + 1; jj < nItem; jj++) {
      int right = itemWeight(pRDtree, &aItem[jj]);
      int growth = itemGrowth(pRDtree, &aItem[ii], &aItem[jj]);
      int waste = growth - right;

      if (waste > iMaxWaste) {
        iLeftSeed = ii;
        iRightSeed = jj;
        iMaxWaste = waste;
      }
    }
  }

  *piLeftSeed = iLeftSeed;
  *piRightSeed = iRightSeed;
}

static int assignItems(RDtree *pRDtree, RDtreeItem *aItem, int nItem,
		       RDtreeNode *pLeft, RDtreeNode *pRight,
		       RDtreeItem *pLeftBounds, RDtreeItem *pRightBounds)
{
  int iLeftSeed = 0;
  int iRightSeed = 1;
  int i;

  int *aiUsed = sqlite3_malloc(sizeof(int) * nItem);
  if (!aiUsed) {
    return SQLITE_NOMEM;
  }
  memset(aiUsed, 0, sizeof(int) * nItem);

  pickSeeds(pRDtree, aItem, nItem, &iLeftSeed, &iRightSeed);

  memcpy(pLeftBounds, &aItem[iLeftSeed], sizeof(RDtreeItem));
  memcpy(pRightBounds, &aItem[iRightSeed], sizeof(RDtreeItem));
  nodeInsertItem(pRDtree, pLeft, &aItem[iLeftSeed]);
  nodeInsertItem(pRDtree, pRight, &aItem[iRightSeed]);
  aiUsed[iLeftSeed] = 1;
  aiUsed[iRightSeed] = 1;

  for(i = nItem - 2; i > 0; i--) {
    RDtreeItem *pNext;
    pNext = pickNext(pRDtree, aItem, nItem, pLeftBounds, pRightBounds, aiUsed);

    int diff =  
      itemGrowth(pRDtree, pLeftBounds, pNext) - 
      itemGrowth(pRDtree, pRightBounds, pNext)
      ;

    /* TODO investigate and understand */
    if ((RDTREE_MINITEMS(pRDtree) - NITEM(pRight) == i)
	|| (diff > 0 && (RDTREE_MINITEMS(pRDtree) - NITEM(pLeft) != i))) {
      nodeInsertItem(pRDtree, pRight, pNext);
      itemUnion(pRDtree, pRightBounds, pNext);
    }
    else {
      nodeInsertItem(pRDtree, pLeft, pNext);
      itemUnion(pRDtree, pLeftBounds, pNext);
    }
  }

  sqlite3_free(aiUsed);
  return SQLITE_OK;
}

static int updateMapping(RDtree *pRDtree, i64 iRowid, 
			 RDtreeNode *pNode, int iHeight)
{
  int (*xSetMapping)(RDtree *, sqlite3_int64, sqlite3_int64);
  xSetMapping = ((iHeight == 0) ? rowidWrite : parentWrite);

  if (iHeight > 0) {
    RDtreeNode *pChild = nodeHashLookup(pRDtree, iRowid);
    if (pChild) {
      nodeRelease(pRDtree, pChild->pParent);
      nodeReference(pNode);
      pChild->pParent = pNode;
    }
  }
  return xSetMapping(pRDtree, iRowid, pNode->iNode);
}

/* forward decl */
static int rdtreeInsertItem(RDtree *, RDtreeNode *, RDtreeItem *, int);

static int splitNode(RDtree *pRDtree, RDtreeNode *pNode, RDtreeItem *pItem,
		     int iHeight)
{
  int i;
  int newItemIsRight = 0;

  int rc = SQLITE_OK;
  int nItem = NITEM(pNode);
  RDtreeItem *aItem;
  int *aiUsed;

  RDtreeNode *pLeft = 0;
  RDtreeNode *pRight = 0;

  RDtreeItem leftbounds;
  RDtreeItem rightbounds;

  /* Allocate an array and populate it with a copy of pItem and 
  ** all items from node pLeft. Then zero the original node.
  ** 
  ** Actually, the same buffer will host both the above mentioned array
  ** (aItem) and an array of integer flags (aiUsed).
  */
  aItem = sqlite3_malloc((sizeof(RDtreeItem) + sizeof(int)) * (nItem + 1));
  if (!aItem) {
    rc = SQLITE_NOMEM;
    goto splitnode_out;
  }
  aiUsed = (int *)&aItem[nItem + 1];
  memset(aiUsed, 0, (sizeof(int) * (nItem + 1)));
  for (i = 0; i < nItem; i++){
    nodeGetItem(pRDtree, pNode, i, &aItem[i]);
  }
  nodeZero(pRDtree, pNode);
  memcpy(&aItem[nItem], pItem, sizeof(RDtreeItem));
  nItem += 1;

  if (pNode->iNode == 1) { /* splitting the root node */
    pRight = nodeNew(pRDtree, pNode);
    pLeft = nodeNew(pRDtree, pNode);
    pRDtree->iDepth++;
    pNode->isDirty = 1;
    writeInt16(pNode->zData, pRDtree->iDepth);
  }
  else {
    pLeft = pNode;
    pRight = nodeNew(pRDtree, pLeft->pParent);
    nodeReference(pLeft);
  }

  if (!pLeft || !pRight) {
    rc = SQLITE_NOMEM;
    goto splitnode_out;
  }

  memset(pLeft->zData, 0, pRDtree->iNodeSize);
  memset(pRight->zData, 0, pRDtree->iNodeSize);

  rc = assignItems(pRDtree, aItem, nItem, pLeft, pRight, 
		   &leftbounds, &rightbounds);

  if (rc != SQLITE_OK) {
    goto splitnode_out;
  }

  /* Ensure both child nodes have node numbers assigned to them by calling
  ** nodeWrite(). Node pRight always needs a node number, as it was created
  ** by nodeNew() above. But node pLeft sometimes already has a node number.
  ** In this case avoid the call to nodeWrite().
  */
  if ((SQLITE_OK != (rc = nodeWrite(pRDtree, pRight))) || 
      (0 == pLeft->iNode && SQLITE_OK != (rc = nodeWrite(pRDtree, pLeft)))) {
    goto splitnode_out;
  }

  rightbounds.iRowid = pRight->iNode;
  leftbounds.iRowid = pLeft->iNode;

  if (pNode->iNode == 1) {
    rc = rdtreeInsertItem(pRDtree, pLeft->pParent, &leftbounds, iHeight+1);
    if (rc != SQLITE_OK) {
      goto splitnode_out;
    }
  }
  else {
    RDtreeNode *pParent = pLeft->pParent;
    int iItem;
    rc = nodeParentIndex(pRDtree, pLeft, &iItem);
    if (rc == SQLITE_OK) {
      nodeOverwriteItem(pRDtree, pParent, &leftbounds, iItem);
      rc = adjustTree(pRDtree, pParent, &leftbounds);
    }
    if (rc != SQLITE_OK) {
      goto splitnode_out;
    }
  }

  if ((rc = 
       rdtreeInsertItem(pRDtree, pRight->pParent, &rightbounds, iHeight+1))) {
    goto splitnode_out;
  }

  for (i = 0; i < NITEM(pRight); i++) {
    i64 iRowid = nodeGetRowid(pRDtree, pRight, i);
    rc = updateMapping(pRDtree, iRowid, pRight, iHeight);
    if (iRowid == pItem->iRowid) {
      newItemIsRight = 1;
    }
    if (rc != SQLITE_OK) {
      goto splitnode_out;
    }
  }

  if (pNode->iNode == 1) {
    for (i = 0; i < NITEM(pLeft); i++) {
      i64 iRowid = nodeGetRowid(pRDtree, pLeft, i);
      rc = updateMapping(pRDtree, iRowid, pLeft, iHeight);
      if (rc != SQLITE_OK) {
        goto splitnode_out;
      }
    }
  }
  else if (newItemIsRight == 0) {
    rc = updateMapping(pRDtree, pItem->iRowid, pLeft, iHeight);
  }

  /* FIXME the following two blocks seem useless to me */

  if (rc == SQLITE_OK) {
    rc = nodeRelease(pRDtree, pRight);
    pRight = 0;
  }

  if (rc == SQLITE_OK) {
    rc = nodeRelease(pRDtree, pLeft);
    pLeft = 0;
  }

splitnode_out:
  nodeRelease(pRDtree, pRight);
  nodeRelease(pRDtree, pLeft);
  sqlite3_free(aItem);
  return rc;
}

/*
** If node pLeaf is not the root of the rd-tree and its pParent pointer is 
** still NULL, load all ancestor nodes of pLeaf into memory and populate
** the pLeaf->pParent chain all the way up to the root node.
**
** This operation is required when a row is deleted (or updated - an update
** is implemented as a delete followed by an insert). SQLite provides the
** rowid of the row to delete, which can be used to find the leaf on which
** the entry resides (argument pLeaf). Once the leaf is located, this 
** function is called to determine its ancestry.
*/
static int fixLeafParent(RDtree *pRDtree, RDtreeNode *pLeaf)
{
  int rc = SQLITE_OK;
  RDtreeNode *pChild = pLeaf;
  while (rc == SQLITE_OK && pChild->iNode != 1 && pChild->pParent == 0) {
    int rc2 = SQLITE_OK;          /* sqlite3_reset() return code */
    sqlite3_bind_int64(pRDtree->pReadParent, 1, pChild->iNode);
    rc = sqlite3_step(pRDtree->pReadParent);
    if (rc == SQLITE_ROW) {
      RDtreeNode *pTest;           /* Used to test for reference loops */
      i64 iNode;                   /* Node number of parent node */

      /* Before setting pChild->pParent, test that we are not creating a
      ** loop of references (as we would if, say, pChild==pParent). We don't
      ** want to do this as it leads to a memory leak when trying to delete
      ** the reference counted node structures.
      */
      iNode = sqlite3_column_int64(pRDtree->pReadParent, 0);
      for (pTest = pLeaf; 
	   pTest && pTest->iNode != iNode; pTest = pTest->pParent)
	; /* loop from pLeaf up towards pChild looking for iNode.. */

      if (!pTest) { /* Ok */
        rc2 = nodeAcquire(pRDtree, iNode, 0, &pChild->pParent);
      }
    }
    rc = sqlite3_reset(pRDtree->pReadParent);
    if (rc == SQLITE_OK) rc = rc2;
    if (rc == SQLITE_OK && !pChild->pParent) rc = SQLITE_CORRUPT_VTAB;
    pChild = pChild->pParent;
  }
  return rc;
}

/* forward decl */
static int deleteItem(RDtree *pRDtree, RDtreeNode *pNode, 
		      int iItem, int iHeight);

static int removeNode(RDtree *pRDtree, RDtreeNode *pNode, int iHeight)
{
  int rc;
  int rc2;
  RDtreeNode *pParent = 0;
  int iItem;

  /* TODO investigate and understand */
  assert( pNode->nRef == 1 );

  /* Remove the entry in the parent item. */
  rc = nodeParentIndex(pRDtree, pNode, &iItem);
  if (rc == SQLITE_OK) {
    pParent = pNode->pParent;
    pNode->pParent = 0;
    rc = deleteItem(pRDtree, pParent, iItem, iHeight+1);
  }
  rc2 = nodeRelease(pRDtree, pParent);
  if (rc == SQLITE_OK) {
    rc = rc2;
  }
  if (rc != SQLITE_OK) {
    return rc;
  }

  /* Remove the xxx_node entry. */
  sqlite3_bind_int64(pRDtree->pDeleteNode, 1, pNode->iNode);
  sqlite3_step(pRDtree->pDeleteNode);
  if (SQLITE_OK != (rc = sqlite3_reset(pRDtree->pDeleteNode))) {
    return rc;
  }

  /* Remove the xxx_parent entry. */
  sqlite3_bind_int64(pRDtree->pDeleteParent, 1, pNode->iNode);
  sqlite3_step(pRDtree->pDeleteParent);
  if (SQLITE_OK != (rc = sqlite3_reset(pRDtree->pDeleteParent))) {
    return rc;
  }
  
  /* Remove the node from the in-memory hash table and link it into
  ** the RDtree.pDeleted list. Its contents will be re-inserted later on.
  */
  nodeHashDelete(pRDtree, pNode);
  pNode->iNode = iHeight;
  pNode->pNext = pRDtree->pDeleted;
  pNode->nRef++;
  pRDtree->pDeleted = pNode;

  return SQLITE_OK;
}

static int fixNodeBounds(RDtree *pRDtree, RDtreeNode *pNode)
{
  int rc = SQLITE_OK; 
  RDtreeNode *pParent = pNode->pParent;
  if (pParent) {
    int ii; 
    int nItem = NITEM(pNode);
    RDtreeItem bounds;  /* Bounding box for pNode */
    nodeGetItem(pRDtree, pNode, 0, &bounds);
    for (ii = 1; ii < nItem; ii++) {
      RDtreeItem item;
      nodeGetItem(pRDtree, pNode, ii, &item);
      itemUnion(pRDtree, &bounds, &item);
    }
    bounds.iRowid = pNode->iNode;
    rc = nodeParentIndex(pRDtree, pNode, &ii);
    if (rc == SQLITE_OK) {
      nodeOverwriteItem(pRDtree, pParent, &bounds, ii);
      rc = fixNodeBounds(pRDtree, pParent);
    }
  }
  return rc;
}

/*
** Delete the item at index iItem of node pNode. After removing the
** item, adjust the rd-tree data structure if required.
*/
static int deleteItem(RDtree *pRDtree, RDtreeNode *pNode, 
		      int iItem, int iHeight)
{
  RDtreeNode *pParent;
  int rc;

  /*
  ** If node is not the root and its parent is null, load all the ancestor
  ** nodes into memory
  */
  if ((rc = fixLeafParent(pRDtree, pNode)) != SQLITE_OK) {
    return rc;
  }

  /* Remove the item from the node. This call just moves bytes around
  ** the in-memory node image, so it cannot fail.
  */
  nodeDeleteItem(pRDtree, pNode, iItem);

  /* If the node is not the tree root and now has less than the minimum
  ** number of cells, remove it from the tree. Otherwise, update the
  ** cell in the parent node so that it tightly contains the updated
  ** node.
  */
  pParent = pNode->pParent;
  assert(pParent || pNode->iNode == 1);
  if (pParent) {
    if (NITEM(pNode) < RDTREE_MINITEMS(pRDtree)) {
      rc = removeNode(pRDtree, pNode, iHeight);
    }
    else {
      rc = fixNodeBounds(pRDtree, pNode);
    }
  }

  return rc;
}


/*
** Insert item pItem into node pNode. Node pNode is the head of a 
** subtree iHeight high (leaf nodes have iHeight==0).
*/
static int rdtreeInsertItem(RDtree *pRDtree, RDtreeNode *pNode,
			    RDtreeItem *pItem, int iHeight)
{
  int rc = SQLITE_OK;

  if (iHeight > 0) {
    RDtreeNode *pChild = nodeHashLookup(pRDtree, pItem->iRowid);
    if (pChild) {
      nodeRelease(pRDtree, pChild->pParent);
      nodeReference(pNode);
      pChild->pParent = pNode;
    }
  }

  if (nodeInsertItem(pRDtree, pNode, pItem)) {
    /* node was full */
    rc = splitNode(pRDtree, pNode, pItem, iHeight);
  }
  else {
    /* insertion succeded */
    rc = adjustTree(pRDtree, pNode, pItem);
    if (rc == SQLITE_OK) {
      if (iHeight == 0) {
        rc = rowidWrite(pRDtree, pItem->iRowid, pNode->iNode);
      }
      else {
        rc = parentWrite(pRDtree, pItem->iRowid, pNode->iNode);
      }
    }
  }

  return rc;
}

static int reinsertNodeContent(RDtree *pRDtree, RDtreeNode *pNode)
{
  int ii;
  int rc = SQLITE_OK;
  int nItem = NITEM(pNode);

  for (ii = 0; rc == SQLITE_OK && ii < nItem; ii++) {
    RDtreeItem item;
    nodeGetItem(pRDtree, pNode, ii, &item);

    /* Find a node to store this cell in. pNode->iNode currently contains
    ** the height of the sub-tree headed by the cell.
    */
    RDtreeNode *pInsert;
    rc = chooseLeaf(pRDtree, &item, (int)pNode->iNode, &pInsert);
    if (rc == SQLITE_OK) {
      int rc2;
      rc = rdtreeInsertItem(pRDtree, pInsert, &item, (int)pNode->iNode);
      rc2 = nodeRelease(pRDtree, pInsert);
      if (rc==SQLITE_OK) {
        rc = rc2;
      }
    }
  }
  return rc;
}

/*
** Select a currently unused rowid for a new rd-tree record.
*/
static int newRowid(RDtree *pRDtree, i64 *piRowid)
{
  int rc;
  sqlite3_bind_null(pRDtree->pWriteRowid, 1);
  sqlite3_bind_null(pRDtree->pWriteRowid, 2);
  sqlite3_step(pRDtree->pWriteRowid);
  rc = sqlite3_reset(pRDtree->pWriteRowid);
  *piRowid = sqlite3_last_insert_rowid(pRDtree->db);
  return rc;
}


/*
** Remove the entry with rowid=iDelete from the rd-tree structure.
*/
static int rdtreeDeleteRowid(RDtree *pRDtree, sqlite3_int64 iDelete) 
{
  int rc, rc2;                    /* Return code */
  RDtreeNode *pLeaf = 0;          /* Leaf node containing record iDelete */
  int iItem;                      /* Index of iDelete item in pLeaf */
  RDtreeNode *pRoot;              /* Root node of rtree structure */


  /* Obtain a reference to the root node to initialise RDtree.iDepth */
  rc = nodeAcquire(pRDtree, 1, 0, &pRoot);

  /* Obtain a reference to the leaf node that contains the entry 
  ** about to be deleted. 
  */
  if (rc == SQLITE_OK) {
    rc = findLeafNode(pRDtree, iDelete, &pLeaf);
  }

  /* Delete the cell in question from the leaf node. */
  if (rc == SQLITE_OK) {
    rc = nodeRowidIndex(pRDtree, pLeaf, iDelete, &iItem);
    if (rc == SQLITE_OK) {
      rc = deleteItem(pRDtree, pLeaf, iItem, 0);
    }
    rc2 = nodeRelease(pRDtree, pLeaf);
    if (rc == SQLITE_OK) {
      rc = rc2;
    }
  }

  /* Delete the corresponding entry in the <rdtree>_rowid table. */
  if (rc == SQLITE_OK) {
    sqlite3_bind_int64(pRDtree->pDeleteRowid, 1, iDelete);
    sqlite3_step(pRDtree->pDeleteRowid);
    rc = sqlite3_reset(pRDtree->pDeleteRowid);
  }

  /* Check if the root node now has exactly one child. If so, remove
  ** it, schedule the contents of the child for reinsertion and 
  ** reduce the tree height by one.
  **
  ** This is equivalent to copying the contents of the child into
  ** the root node (the operation that Gutman's paper says to perform 
  ** in this scenario).
  */
  if (rc == SQLITE_OK && pRDtree->iDepth > 0 && NITEM(pRoot) == 1) {
    RDtreeNode *pChild;
    i64 iChild = nodeGetRowid(pRDtree, pRoot, 0);
    rc = nodeAcquire(pRDtree, iChild, pRoot, &pChild);
    if( rc==SQLITE_OK ){
      rc = removeNode(pRDtree, pChild, pRDtree->iDepth - 1);
    }
    rc2 = nodeRelease(pRDtree, pChild);
    if (rc == SQLITE_OK) {
      rc = rc2;
    }
    if (rc == SQLITE_OK) {
      pRDtree->iDepth--;
      writeInt16(pRoot->zData, pRDtree->iDepth);
      pRoot->isDirty = 1;
    }
  }

  /* Re-insert the contents of any underfull nodes removed from the tree. */
  for (pLeaf = pRDtree->pDeleted; pLeaf; pLeaf = pRDtree->pDeleted) {
    if (rc == SQLITE_OK) {
      rc = reinsertNodeContent(pRDtree, pLeaf);
    }
    pRDtree->pDeleted = pLeaf->pNext;
    sqlite3_free(pLeaf);
  }

  /* Release the reference to the root node. */
  rc2 = nodeRelease(pRDtree, pRoot);
  if (rc == SQLITE_OK) {
    rc = rc2;
  }

  return rc;
}


/*
** The xUpdate method for rdtree module virtual tables.
*/
static int rdtreeUpdate(sqlite3_vtab *pVtab, 
			int argc, sqlite3_value **argv, 
			sqlite_int64 *pRowid)
{
  RDtree *pRDtree = (RDtree *)pVtab;
  int rc = SQLITE_OK;
  RDtreeItem *pItem;              /* New item to insert if argc>1 */
  int bHaveRowid = 0;             /* Set to 1 after new rowid is determined */

  rdtreeReference(pRDtree);
  assert(argc >= 1);

  /* Constraint handling. A write operation on an rd-tree table may return
  ** SQLITE_CONSTRAINT in case of a duplicate rowid value or in case the 
  ** argument type doesn't correspond to a binary fingerprint.
  **
  ** TODO/FIXME also enforce the correct number of arguments.
  **
  ** In the case of duplicate rowid, if the conflict-handling mode is REPLACE,
  ** then the conflicting row can be removed before proceeding.
  */

  /* If the argv[] array contains more than one element, elements
  ** (argv[2]..argv[argc-1]) contain a new record to insert into
  ** the rd-tree structure.
  */
  if (argc > 1) { 

    i64 rowid = 0;

    /* If a rowid value was supplied, check if it is already present in 
    ** the table. If so, the constraint has failed. */
    if (sqlite3_value_type(argv[1]) != SQLITE_NULL ) {

      rowid = sqlite3_value_int64(argv[1]);

      if ((sqlite3_value_type(argv[0]) == SQLITE_NULL) ||
	  (sqlite3_value_int64(argv[0]) != rowid)) {
        sqlite3_bind_int64(pRDtree->pReadRowid, 1, rowid);
        int steprc = sqlite3_step(pRDtree->pReadRowid);
        rc = sqlite3_reset(pRDtree->pReadRowid);
        if (SQLITE_ROW == steprc) { /* rowid already exists */
          if (sqlite3_vtab_on_conflict(pRDtree->db) == SQLITE_REPLACE) {
            rc = rdtreeDeleteRowid(pRDtree, rowid);
          }
	  else {
            rc = SQLITE_CONSTRAINT;
            goto update_end;
          }
        }
      }

      bHaveRowid = 1;
    }

    /* Allocate the RDtreeItem and copy the binary fingerprint data  */
    pItem = (RDtreeItem *)sqlite3_malloc(sizeof(RDtreeItem));

    Bfp *pBfp = 0;
    u8 *pBlob = 0; int len;

    if (!pItem) {
      rc = SQLITE_NOMEM;
      goto update_end;
    }
    else if ((rc = fetch_bfp_arg(argv[2], &pBfp)) != SQLITE_OK) {
      goto update_build_item_end;
    }
    else if ((rc = bfp_to_blob(pBfp, &pBlob, &len)) != SQLITE_OK) {
      goto update_free_bfp;
    }
    else if (len != pRDtree->iBfpSize) {
      rc = SQLITE_CONSTRAINT;
      goto update_free_blob;
    }
    else {
      if (bHaveRowid) {
	pItem->iRowid = rowid;
      }
      memcpy(pItem->aBfp, pBlob, pRDtree->iBfpSize);
    }

  update_free_blob:
    sqlite3_free(pBlob);
  update_free_bfp:
    free_bfp(pBfp);
  update_build_item_end:
    if (rc != SQLITE_OK) {
      goto update_end;
    }
  }

  /* If argv[0] is not an SQL NULL value, it is the rowid of a
  ** record to delete from the r-tree table. The following block does
  ** just that.
  */
  if (sqlite3_value_type(argv[0]) != SQLITE_NULL) {
    rc = rdtreeDeleteRowid(pRDtree, sqlite3_value_int64(argv[0]));
  }

  /* If the argv[] array contains more than one element, elements
  ** (argv[2]..argv[argc-1]) contain a new record to insert into
  ** the rd-tree structure.
  */
  if ((rc == SQLITE_OK) && (argc > 1)) {
    /* Insert the new record into the rd-tree */
    RDtreeNode *pLeaf = 0;

    /* Figure out the rowid of the new row. */
    if (bHaveRowid == 0) {
      rc = newRowid(pRDtree, &pItem->iRowid);
    }
    *pRowid = pItem->iRowid;

    if (rc == SQLITE_OK) {
      rc = chooseLeaf(pRDtree, pItem, 0, &pLeaf);
    }

    if (rc == SQLITE_OK) {
      int rc2;
      pRDtree->iReinsertHeight = -1;
      rc = rdtreeInsertItem(pRDtree, pLeaf, pItem, 0);
      rc2 = nodeRelease(pRDtree, pLeaf);
      if (rc == SQLITE_OK) {
        rc = rc2;
      }
    }
  }

  /* FIXME free pItem or done elsewhere ? */

update_end:
  rdtreeRelease(pRDtree);
  return rc;
}


/*
** The xRename method for rd-tree module virtual tables.
*/
static int rdtreeRename(sqlite3_vtab *pVtab, const char *zNewName)
{
  RDtree *pRDtree = (RDtree *)pVtab;
  int rc = SQLITE_NOMEM;
  char *zSql = sqlite3_mprintf(
    "ALTER TABLE %Q.'%q_node'   RENAME TO \"%w_node\";"
    "ALTER TABLE %Q.'%q_parent' RENAME TO \"%w_parent\";"
    "ALTER TABLE %Q.'%q_rowid'  RENAME TO \"%w_rowid\";"
    , pRDtree->zDb, pRDtree->zName, zNewName 
    , pRDtree->zDb, pRDtree->zName, zNewName 
    , pRDtree->zDb, pRDtree->zName, zNewName
  );
  if (zSql) {
    rc = sqlite3_exec(pRDtree->db, zSql, 0, 0, 0);
    sqlite3_free(zSql);
  }
  return rc;
}


static sqlite3_module rdtreeModule = {
  0,                           /* iVersion */
  rdtreeCreate,                /* xCreate - create a table */
  rdtreeConnect,               /* xConnect - connect to an existing table */
  0, /* rdtreeBestIndex,             /* xBestIndex - Determine search strategy */
  rdtreeDisconnect,            /* xDisconnect - Disconnect from a table */
  rdtreeDestroy,               /* xDestroy - Drop a table */
  0, /* rdtreeOpen,                  /* xOpen - open a cursor */
  0, /* rdtreeClose,                 /* xClose - close a cursor */
  0, /* rdtreeFilter,                /* xFilter - configure scan constraints */
  0, /* rdtreeNext,                  /* xNext - advance a cursor */
  0, /* rdtreeEof,                   /* xEof */
  0, /* rdtreeColumn,                /* xColumn - read data */
  0, /* rdtreeRowid,                 /* xRowid - read data */
  0, /* rdtreeUpdate,                /* xUpdate - write data */
  0,                           /* xBegin - begin transaction */
  0,                           /* xSync - sync transaction */
  0,                           /* xCommit - commit transaction */
  0,                           /* xRollback - rollback transaction */
  0,                           /* xFindFunction - function overloading */
  rdtreeRename,                /* xRename - rename the table */
  0,                           /* xSavepoint */
  0,                           /* xRelease */
  0                            /* xRollbackTo */
};

static int rdtreeSqlInit(RDtree *pRDtree, int isCreate)
{
  int rc = SQLITE_OK;

  #define N_STATEMENT 9
  static const char *azSql[N_STATEMENT] = {
    /* Read and write the xxx_node table */
    "SELECT data FROM '%q'.'%q_node' WHERE nodeno = :1",
    "INSERT OR REPLACE INTO '%q'.'%q_node' VALUES(:1, :2)",
    "DELETE FROM '%q'.'%q_node' WHERE nodeno = :1",

    /* Read and write the xxx_rowid table */
    "SELECT nodeno FROM '%q'.'%q_rowid' WHERE rowid = :1",
    "INSERT OR REPLACE INTO '%q'.'%q_rowid' VALUES(:1, :2)",
    "DELETE FROM '%q'.'%q_rowid' WHERE rowid = :1",

    /* Read and write the xxx_parent table */
    "SELECT parentnode FROM '%q'.'%q_parent' WHERE nodeno = :1",
    "INSERT OR REPLACE INTO '%q'.'%q_parent' VALUES(:1, :2)",
    "DELETE FROM '%q'.'%q_parent' WHERE nodeno = :1"
  };
  sqlite3_stmt **appStmt[N_STATEMENT];
  int i;

  if (isCreate) {
    char *zCreate 
      = sqlite3_mprintf("CREATE TABLE \"%w\".\"%w_node\"(nodeno INTEGER PRIMARY KEY, data BLOB);"
			"CREATE TABLE \"%w\".\"%w_rowid\"(rowid INTEGER PRIMARY KEY, nodeno INTEGER);"
			"CREATE TABLE \"%w\".\"%w_parent\"(nodeno INTEGER PRIMARY KEY, parentnode INTEGER);"
			"INSERT INTO '%q'.'%q_node' VALUES(1, zeroblob(%d))",
			pRDtree->zDb, pRDtree->zName, 
			pRDtree->zDb, pRDtree->zName, 
			pRDtree->zDb, pRDtree->zName, 
			pRDtree->zDb, pRDtree->zName, 
			pRDtree->iNodeSize
    );
    if (!zCreate) {
      return SQLITE_NOMEM;
    }
    rc = sqlite3_exec(pRDtree->db, zCreate, 0, 0, 0);
    sqlite3_free(zCreate);
    if (rc != SQLITE_OK) {
      return rc;
    }
  }

  appStmt[0] = &pRDtree->pReadNode;
  appStmt[1] = &pRDtree->pWriteNode;
  appStmt[2] = &pRDtree->pDeleteNode;
  appStmt[3] = &pRDtree->pReadRowid;
  appStmt[4] = &pRDtree->pWriteRowid;
  appStmt[5] = &pRDtree->pDeleteRowid;
  appStmt[6] = &pRDtree->pReadParent;
  appStmt[7] = &pRDtree->pWriteParent;
  appStmt[8] = &pRDtree->pDeleteParent;

  for (i=0; i<N_STATEMENT && rc==SQLITE_OK; i++) {
    char *zSql = sqlite3_mprintf(azSql[i], pRDtree->zDb, pRDtree->zName);
    if (zSql) {
      rc = sqlite3_prepare_v2(pRDtree->db, zSql, -1, appStmt[i], 0); 
    }
    else {
      rc = SQLITE_NOMEM;
    }
    sqlite3_free(zSql);
  }
  
  return rc;
}

/*
** The second argument to this function contains the text of an SQL statement
** that returns a single integer value. The statement is compiled and executed
** using database connection db. If successful, the integer value returned
** is written to *piVal and SQLITE_OK returned. Otherwise, an SQLite error
** code is returned and the value of *piVal after returning is not defined.
*/
static int getIntFromStmt(sqlite3 *db, const char *zSql, int *piVal)
{
  int rc = SQLITE_NOMEM;
  if (zSql) {
    sqlite3_stmt *pStmt = 0;
    rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
    if (rc == SQLITE_OK) {
      if (SQLITE_ROW == sqlite3_step(pStmt)) {
        *piVal = sqlite3_column_int(pStmt, 0);
      }
      rc = sqlite3_finalize(pStmt);
    }
  }
  return rc;
}


/*
** This function is called from within the xConnect() or xCreate() method to
** determine the node-size used by the rdtree table being created or connected
** to. If successful, pRDtree->iNodeSize is populated and SQLITE_OK returned.
** Otherwise, an SQLite error code is returned.
**
** If this function is being called as part of an xConnect(), then the rdtree
** table already exists. In this case the node-size is determined by inspecting
** the root node of the tree.
**
** Otherwise, for an xCreate(), use 64 bytes less than the database page-size. 
** This ensures that each node is stored on a single database page. If the 
** database page-size is so large that more than RDTREE_MAXITEMS entries 
** would fit in a single node, use a smaller node-size.
*/
static int getNodeSize(RDtree *pRDtree, int isCreate)
{
  int rc;
  char *zSql;
  if (isCreate) {
    int iPageSize = 0;
    zSql = sqlite3_mprintf("PRAGMA %Q.page_size", pRDtree->zDb);
    rc = getIntFromStmt(pRDtree->db, zSql, &iPageSize);
    if (rc==SQLITE_OK) {
      pRDtree->iNodeSize = iPageSize - 64;
      if ((4 + pRDtree->nBytesPerItem*RDTREE_MAXITEMS) < pRDtree->iNodeSize) {
        pRDtree->iNodeSize = 4 + pRDtree->nBytesPerItem*RDTREE_MAXITEMS;
      }
    }
  }
  else{
    zSql = sqlite3_mprintf("SELECT length(data) FROM '%q'.'%q_node' "
			   "WHERE nodeno=1", pRDtree->zDb, pRDtree->zName);
    rc = getIntFromStmt(pRDtree->db, zSql, &pRDtree->iNodeSize);
  }

  sqlite3_free(zSql);
  return rc;
}

/* 
** This function is the implementation of both the xConnect and xCreate
** methods of the rd-tree virtual table.
**
**   argv[0]   -> module name
**   argv[1]   -> database name
**   argv[2]   -> table name
**   argv[...] -> columns spec...
*/
static int rdtreeInit(sqlite3 *db, void *pAux,
		      int argc, const char *const*argv,
		      sqlite3_vtab **ppVtab, char **pzErr,
		      int isCreate)
{
  int rc = SQLITE_OK;
  RDtree *pRDtree;
  int nDb;              /* Length of string argv[1] */
  int nName;            /* Length of string argv[2] */

  int iBfpSize = 128;   /* Default size of binary fingerprint */

  /* perform arg checking */
  if (argc != 5) {
    *pzErr = sqlite3_mprintf("wrong number of arguments. "
                             "two column definitions expected.");
    return SQLITE_ERROR;
  }

  int sz;
  if (sscanf(argv[4], "%*s bytes( %d )", &sz) == 1) {
      iBfpSize = sz;
  }

  sqlite3_vtab_config(db, SQLITE_VTAB_CONSTRAINT_SUPPORT, 1); /* WAT? FIXME */

  /* Allocate the sqlite3_vtab structure */
  nDb = (int)strlen(argv[1]);
  nName = (int)strlen(argv[2]);
  pRDtree = (RDtree *)sqlite3_malloc(sizeof(RDtree)+nDb+nName+2);
  if (!pRDtree) {
    return SQLITE_NOMEM;
  }
  memset(pRDtree, 0, sizeof(RDtree)+nDb+nName+2);

  pRDtree->db = db;
  pRDtree->iBfpSize = iBfpSize;
  pRDtree->nBytesPerItem = 8 + iBfpSize; 
  pRDtree->nBusy = 1;
  pRDtree->zDb = (char *)&pRDtree[1];
  pRDtree->zName = &pRDtree->zDb[nDb+1];
  memcpy(pRDtree->zDb, argv[1], nDb);
  memcpy(pRDtree->zName, argv[2], nName);

  /* Figure out the node size to use. */
  rc = getNodeSize(pRDtree, isCreate);

  /* Create/Connect to the underlying relational database schema. If
  ** that is successful, call sqlite3_declare_vtab() to configure
  ** the rd-tree table schema.
  */
  if (rc == SQLITE_OK) {
    if ((rc = rdtreeSqlInit(pRDtree, isCreate))) {
      *pzErr = sqlite3_mprintf("%s", sqlite3_errmsg(db));
    } 
    else {
      char *zSql = sqlite3_mprintf("CREATE TABLE x(%s", argv[3]);
      char *zTmp;
      int ii;
      for(ii=4; zSql && ii<argc; ii++){
        zTmp = zSql;
        zSql = sqlite3_mprintf("%s, %s", zTmp, argv[ii]);
        sqlite3_free(zTmp);
      }
      if( zSql ){
        zTmp = zSql;
        zSql = sqlite3_mprintf("%s);", zTmp);
        sqlite3_free(zTmp);
      }
      if( !zSql ){
        rc = SQLITE_NOMEM;
      }else if( SQLITE_OK!=(rc = sqlite3_declare_vtab(db, zSql)) ){
        *pzErr = sqlite3_mprintf("%s", sqlite3_errmsg(db));
      }
      sqlite3_free(zSql);
    }
  }

  if( rc==SQLITE_OK ){
    *ppVtab = (sqlite3_vtab *)pRDtree;
  }else{
    rdtreeRelease(pRDtree);
  }

  return rc;
}


int chemicalite_init_rdtree(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) {
    rc = sqlite3_create_module_v2(db, "rdtree", &rdtreeModule, 
				  0,  /* Client data for xCreate/xConnect */
				  0   /* Module destructor function */
				  );
  }

  return rc;
}
