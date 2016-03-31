#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "chemicalite.h"
#include "bfp_ops.h"
#include "bitstring.h"
#include "utils.h"
#include "rdtree.h"

/* 
** This index data structure is based on (and *very* similar to) the
** SQLite's r-tree and r*-tree implementation, with the necessary variations
** on the algorithms and search constraints definition which were required to
** turn it into an rd-tree.
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
typedef struct RDtreeCursor RDtreeCursor;
typedef struct RDtreeConstraint RDtreeConstraint;
typedef struct RDtreeMatchOp RDtreeMatchOp;
typedef struct RDtreeMatchArg RDtreeMatchArg;
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
** Functions to serialize a 16 bit and 64 bit integer.
** The value returned is the number of bytes written
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
  int iNodeCapacity;           /* Size (items) of each node */
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
#define RDTREE_MINITEMS(p) ((p)->iNodeCapacity/3)
#define RDTREE_MAXITEMS 51

/*
** The largest supported item size is 264 bytes (8 byte rowid + 256 bytes 
** fingerprint). Assuming that a database page size of at least 2048 bytes is
** in use, then all non-root nodes must contain at least 2 entries.
** An rd-tree structure therefore always has a depth of 64 or less.
**
** The same proportion holds for the default bfp size (128 bytes) and the 
** most common database default page size on unix workstations (1024 bytes).
**
** While these look like the minimal lower bounds, the optimal page size is to 
** be investigated.
*/
#define RDTREE_MAX_DEPTH 64

/* 
** An rdtree cursor object.
*/
struct RDtreeCursor {
  sqlite3_vtab_cursor base;
  RDtreeNode *pNode;                /* Node cursor is currently pointing at */
  int iItem;                        /* Index of current item in pNode */
  int iStrategy;                    /* Copy of idxNum search parameter */
  int nConstraint;                  /* Number of entries in aConstraint */
  RDtreeConstraint *aConstraint;    /* Search constraints. */
};

/*
** A bitstring search constraint.
*/
struct RDtreeConstraint {
  /* Ok this part is a bit ugly and these data structures will benefit some
  ** redesign. FIXME
  */
  u8 aBfp[MAX_BITSTRING_SIZE];    /* Constraint value. */
  int iWeight;
  double dParam;
  RDtreeMatchOp *op;
};

/*
** Value for the first field of every RDtreeMatchArg object. The MATCH
** operator tests that the first field of a blob operand matches this
** value to avoid operating on invalid blobs (which could cause a segfault).
*/
#define RDTREE_MATCH_MAGIC 0xA355FA97

/*
** An instance of this structure must be supplied as a blob argument to
** the right-hand-side of an SQL MATCH operator used to constrain an
** rd-tree query.
*/
struct RDtreeMatchArg {
  u32 magic;                      /* Always RDTREE_MATCH_MAGIC */
  RDtreeConstraint constraint;
};

struct RDtreeMatchOp {
  int (*xTestInternal)(RDtree*, RDtreeConstraint*, RDtreeItem*, int*); 
  int (*xTestLeaf)(RDtree*, RDtreeConstraint*, RDtreeItem*, int*);
};

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

/* access the number of items stored by the node */
#define NITEM(pNode) readInt16(&(pNode)->zData[2])

/* 
** Structure to store a deserialized rd-tree record.
*/
struct RDtreeItem {
  i64 iRowid;
  int iMinWeight;
  int iMaxWeight;
  u8 aBfp[MAX_BITSTRING_SIZE];
};

static int itemWeight(RDtree *pRDtree, RDtreeItem *pItem)
{
  return bfp_op_weight(pRDtree->iBfpSize, pItem->aBfp);
}

/*
** Return true if item p2 is a subset of item p1. False otherwise.
*/
static int itemContains(RDtree *pRDtree, RDtreeItem *p1, RDtreeItem *p2)
{
  return ( p1->iMinWeight <= p2->iMinWeight &&
	   p1->iMaxWeight >= p2->iMaxWeight &&
	   bfp_op_contains(pRDtree->iBfpSize, p1->aBfp, p2->aBfp) );
}

/*
** Return the amount item pBase would grow by if it were unioned with pAdded.
*/
static int itemGrowth(RDtree *pRDtree, RDtreeItem *pBase, RDtreeItem *pAdded)
{
  return bfp_op_growth(pRDtree->iBfpSize, pBase->aBfp, pAdded->aBfp);
}

/*
** Extend the bounds of p1 to contain p2
*/
static void itemExtendBounds(RDtree *pRDtree, RDtreeItem *p1, RDtreeItem *p2)
{
  bfp_op_union(pRDtree->iBfpSize, p1->aBfp, p2->aBfp);
  if (p1->iMinWeight > p2->iMinWeight) { p1->iMinWeight = p2->iMinWeight; }
  if (p1->iMaxWeight < p2->iMaxWeight) { p1->iMaxWeight = p2->iMaxWeight; }
}

/*
** Increment the reference count of node p.
*/
static void nodeReference(RDtreeNode *p)
{
  if (p) {
    p->nRef++; 
  }
}

/*
** Clear the content of node p (set all bytes to 0x00).
*/
static void nodeZero(RDtree *pRDtree, RDtreeNode *p)
{
  assert(p);
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
    if (NITEM(pNode) > pRDtree->iNodeCapacity) {
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
  u8 *p = &pNode->zData[4 + pRDtree->nBytesPerItem*iItem];
  p += writeInt64(p, pItem->iRowid);
  p += writeInt16(p, pItem->iMinWeight);
  p += writeInt16(p, pItem->iMaxWeight);
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
  int nItem = NITEM(pNode);  /* Current number of items in pNode */

  assert(nItem <= pRDtree->iNodeCapacity);

  if (nItem < pRDtree->iNodeCapacity) {
    nodeOverwriteItem(pRDtree, pNode, pItem, nItem);
    writeInt16(&pNode->zData[2], nItem+1);
    pNode->isDirty = 1;
  } 

  return (nItem == pRDtree->iNodeCapacity) ? SQLITE_FULL : SQLITE_OK;
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
  return &pNode->zData[4 + pRDtree->nBytesPerItem*iItem + 8 /* rowid */ + 4 /* min/max weight */];
}

/*
** Deserialize item iItem of node pNode. Populate the structure pointed
** to by pItem with the results.
*/
static void nodeGetItem(RDtree *pRDtree, RDtreeNode *pNode, 
			int iItem, RDtreeItem *pItem)
{
  pItem->iRowid = nodeGetRowid(pRDtree, pNode, iItem);
  pItem->iMinWeight = readInt16(&pNode->zData[4
					      + pRDtree->nBytesPerItem*iItem
					      + 8 /* rowid */]);
  pItem->iMaxWeight = readInt16(&pNode->zData[4
					      + pRDtree->nBytesPerItem*iItem
					      + 8 /* rowid */
					      + 2 /* min weight */]);
  u8 *pBfp = nodeGetBfp(pRDtree, pNode, iItem);
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
** RDtree virtual table module xOpen method.
*/
static int rdtreeOpen(sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor)
{
  int rc = SQLITE_NOMEM;
  RDtreeCursor *pCsr;

  pCsr = (RDtreeCursor *)sqlite3_malloc(sizeof(RDtreeCursor));
  if( pCsr ){
    memset(pCsr, 0, sizeof(RDtreeCursor));
    pCsr->base.pVtab = pVTab;
    rc = SQLITE_OK;
  }
  *ppCursor = (sqlite3_vtab_cursor *)pCsr;

  return rc;
}

/*
** Free the RDtreeCursor.aConstraint[] array and its contents.
*/
static void freeCursorConstraints(RDtreeCursor *pCsr)
{
  if (pCsr->aConstraint) {
    sqlite3_free(pCsr->aConstraint);
    pCsr->aConstraint = 0;
  }
}

/* 
** RDtree virtual table module xClose method.
*/
static int rdtreeClose(sqlite3_vtab_cursor *cur)
{
  RDtree *pRDtree = (RDtree *)(cur->pVtab);
  int rc;
  RDtreeCursor *pCsr = (RDtreeCursor *)cur;
  freeCursorConstraints(pCsr);
  rc = nodeRelease(pRDtree, pCsr->pNode);
  sqlite3_free(pCsr);
  return rc;
}

/*
** RDtree virtual table module xEof method.
**
** Return non-zero if the cursor does not currently point to a valid 
** record (i.e if the scan has finished), or zero otherwise.
*/
static int rdtreeEof(sqlite3_vtab_cursor *cur)
{
  RDtreeCursor *pCsr = (RDtreeCursor *)cur;
  return (pCsr->pNode == 0);
}

static int testRDtreeItem(RDtree *pRDtree, RDtreeCursor *pCursor, int iHeight,
			  int *pbEof)
{
  RDtreeItem item;
  int ii;
  int bEof = 0;
  int rc = SQLITE_OK;

  nodeGetItem(pRDtree, pCursor->pNode, pCursor->iItem, &item);

  for (ii = 0; 
       bEof == 0 && rc == SQLITE_OK && ii < pCursor->nConstraint; ii++) {
    RDtreeConstraint *p = &pCursor->aConstraint[ii];
    int (*xTestItem)(RDtree*, RDtreeConstraint*, RDtreeItem*, int*)
      = (iHeight == 0) ? p->op->xTestLeaf : p->op->xTestInternal;
    rc = xTestItem(pRDtree, p, &item, &bEof);
  }

  *pbEof = bEof;
  return rc;
}

/*
** Cursor pCursor currently points at a node that heads a sub-tree of
** height iHeight (if iHeight==0, then the node is a leaf). Descend
** to point to the left-most cell of the sub-tree that matches the 
** configured constraints.
*/
static int descendToItem(RDtree *pRDtree, 
			 RDtreeCursor *pCursor, 
			 int iHeight,
			 int *pEof) /* OUT: Set to true if cannot descend */
{
  int isEof;
  int rc;
  int ii;
  int nItem;
  RDtreeNode *pChild;
  sqlite3_int64 iRowid;

  RDtreeNode *pSavedNode = pCursor->pNode;
  int iSavedItem = pCursor->iItem;

  assert( iHeight >= 0 );

  rc = testRDtreeItem(pRDtree, pCursor, iHeight, &isEof);

  if (rc != SQLITE_OK || isEof || iHeight==0) {
    goto descend_to_cell_out;
  }

  iRowid = nodeGetRowid(pRDtree, pCursor->pNode, pCursor->iItem);
  rc = nodeAcquire(pRDtree, iRowid, pCursor->pNode, &pChild);
  if (rc != SQLITE_OK) {
    goto descend_to_cell_out;
  }

  nodeRelease(pRDtree, pCursor->pNode);
  pCursor->pNode = pChild;
  isEof = 1;
  nItem = NITEM(pChild);
  for (ii=0; isEof && ii < nItem; ii++) {
    pCursor->iItem = ii;
    rc = descendToItem(pRDtree, pCursor, iHeight-1, &isEof);
    if (rc != SQLITE_OK) {
      goto descend_to_cell_out;
    }
  }

  if (isEof) {
    assert( pCursor->pNode == pChild );
    nodeReference(pSavedNode);
    nodeRelease(pRDtree, pChild);
    pCursor->pNode = pSavedNode;
    pCursor->iItem = iSavedItem;
  }

descend_to_cell_out:
  *pEof = isEof;
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
** RDtree virtual table module xNext method.
*/
static int rdtreeNext(sqlite3_vtab_cursor *pVtabCursor)
{
  RDtree *pRDtree = (RDtree *)(pVtabCursor->pVtab);
  RDtreeCursor *pCsr = (RDtreeCursor *)pVtabCursor;
  int rc = SQLITE_OK;

  /* RDtreeCursor.pNode must not be NULL. If it is NULL, then this cursor is
  ** already at EOF. It is against the rules to call the xNext() method of
  ** a cursor that has already reached EOF.
  */
  assert( pCsr->pNode );

  if (pCsr->iStrategy == 1) {
    /* This "scan" is a direct lookup by rowid. There is no next entry. */
    nodeRelease(pRDtree, pCsr->pNode);
    pCsr->pNode = 0;
  }
  else {
    /* Move to the next entry that matches the configured constraints. */
    int iHeight = 0;
    while (pCsr->pNode) {
      RDtreeNode *pNode = pCsr->pNode;
      int nItem = NITEM(pNode);
      for (pCsr->iItem++; pCsr->iItem < nItem; pCsr->iItem++) {
        int isEof;
        rc = descendToItem(pRDtree, pCsr, iHeight, &isEof);
        if (rc != SQLITE_OK || !isEof) {
          return rc;
        }
      }
      pCsr->pNode = pNode->pParent;
      rc = nodeParentIndex(pRDtree, pNode, &pCsr->iItem);
      if (rc != SQLITE_OK) {
        return rc;
      }
      nodeReference(pCsr->pNode);
      nodeRelease(pRDtree, pNode);
      iHeight++;
    }
  }

  return rc;
}

/* 
** RDtree virtual table module xRowid method.
*/
static int rdtreeRowid(sqlite3_vtab_cursor *pVtabCursor, sqlite_int64 *pRowid)
{
  RDtree *pRDtree = (RDtree *)pVtabCursor->pVtab;
  RDtreeCursor *pCsr = (RDtreeCursor *)pVtabCursor;

  assert(pCsr->pNode);
  *pRowid = nodeGetRowid(pRDtree, pCsr->pNode, pCsr->iItem);

  return SQLITE_OK;
}

/* 
** RDtree virtual table module xColumn method.
*/
static int rdtreeColumn(sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int i)
{
  RDtree *pRDtree = (RDtree *)cur->pVtab;
  RDtreeCursor *pCsr = (RDtreeCursor *)cur;

  if (i == 0) {
    i64 iRowid = nodeGetRowid(pRDtree, pCsr->pNode, pCsr->iItem);
    sqlite3_result_int64(ctx, iRowid);
  }
  else {
    u8 *aBfp = nodeGetBfp(pRDtree, pCsr->pNode, pCsr->iItem);
    sqlite3_result_blob(ctx, aBfp, pRDtree->iBfpSize, NULL); /*SQLITE_STATIC?*/ 
  }

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
** This function is called to configure the RDtreeConstraint object passed
** as the second argument for a MATCH constraint. The value passed as the
** first argument to this function is the right-hand operand to the MATCH
** operator.
*/
static int deserializeMatchArg(sqlite3_value *pValue, RDtreeConstraint *pCons)
{
  RDtreeMatchArg *p;
  int nBlob;

  /* Check that value is actually a blob. */
  if( sqlite3_value_type(pValue) != SQLITE_BLOB ) {
    return SQLITE_ERROR;
  }

  /* Check that the blob is the right size. */
  nBlob = sqlite3_value_bytes(pValue);
  if (nBlob != sizeof(RDtreeMatchArg)) {
    return SQLITE_ERROR;
  }

  RDtreeMatchArg matchArg;
  memcpy(&matchArg, sqlite3_value_blob(pValue), nBlob);

  if (matchArg.magic != RDTREE_MATCH_MAGIC) {
    return SQLITE_ERROR;
  }

  *pCons = matchArg.constraint;

  return SQLITE_OK;
}


/* 
** RDtree virtual table module xFilter method.
*/
static int rdtreeFilter(sqlite3_vtab_cursor *pVtabCursor, 
			int idxNum, const char *idxStr,
			int argc, sqlite3_value **argv)
{
  RDtree *pRDtree = (RDtree *)pVtabCursor->pVtab;
  RDtreeCursor *pCsr = (RDtreeCursor *)pVtabCursor;

  RDtreeNode *pRoot = 0;
  int ii;
  int rc = SQLITE_OK;

  rdtreeReference(pRDtree);

  freeCursorConstraints(pCsr);
  pCsr->iStrategy = idxNum;

  if (idxNum == 1) {
    /* Special case - lookup by rowid. */
    RDtreeNode *pLeaf;        /* Leaf on which the required item resides */
    i64 iRowid = sqlite3_value_int64(argv[0]);
    rc = findLeafNode(pRDtree, iRowid, &pLeaf);
    pCsr->pNode = pLeaf; 
    if (pLeaf) {
      assert(rc == SQLITE_OK);
      rc = nodeRowidIndex(pRDtree, pLeaf, iRowid, &pCsr->iItem);
    }
  }
  else {
    /* Normal case - rd-tree scan. Set up the RDtreeCursor.aConstraint array 
    ** with the configured constraints. 
    */
    if (argc > 0) {
      pCsr->aConstraint = sqlite3_malloc(sizeof(RDtreeConstraint) * argc);
      pCsr->nConstraint = argc;
      if (!pCsr->aConstraint) {
        rc = SQLITE_NOMEM;
      }
      else {
        memset(pCsr->aConstraint, 0, sizeof(RDtreeConstraint) * argc);
        for (ii = 0; ii < argc; ii++) {
          RDtreeConstraint *p = &pCsr->aConstraint[ii];
	  /* A MATCH operator. The right-hand-side must be a blob that
	  ** can be cast into an RDtreeMatchArg object.
	  */
	  rc = deserializeMatchArg(argv[ii], p);
	  if (rc != SQLITE_OK) {
	    break;
	  }
        }
      }
    }
  
    if (rc == SQLITE_OK) {
      pCsr->pNode = 0;
      rc = nodeAcquire(pRDtree, 1, 0, &pRoot);
    }
    if (rc == SQLITE_OK) {
      int isEof = 1;
      int nItem = NITEM(pRoot);
      pCsr->pNode = pRoot;
      for (pCsr->iItem = 0; 
	   rc == SQLITE_OK && pCsr->iItem < nItem; pCsr->iItem++) {
        assert(pCsr->pNode == pRoot);
        rc = descendToItem(pRDtree, pCsr, pRDtree->iDepth, &isEof);
        if (!isEof) {
          break;
        }
      }
      if (rc == SQLITE_OK && isEof) {
        assert( pCsr->pNode == pRoot );
        nodeRelease(pRDtree, pRoot);
        pCsr->pNode = 0;
      }
      assert(rc != SQLITE_OK || !pCsr->pNode || 
	     pCsr->iItem < NITEM(pCsr->pNode));
    }
  }

  rdtreeRelease(pRDtree);
  return rc;
}


/*
** RDtree virtual table module xBestIndex method. There are three
** table scan strategies to choose from (in order from most to 
** least desirable):
**
**   idxNum     idxStr        Strategy
**   ------------------------------------------------
**     1        Unused        Direct lookup by rowid.
**     2        See below     RD-tree query or full-table scan.
**   ------------------------------------------------
*/
static int rdtreeBestIndex(sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo)
{
  int rc = SQLITE_OK;
  int ii;

  assert( pIdxInfo->idxStr==0 );

  for(ii = 0; ii < pIdxInfo->nConstraint; ii++) {

    struct sqlite3_index_constraint *p = &pIdxInfo->aConstraint[ii];

    if (!p->usable) { 
      continue; 
    }
    else if (p->iColumn == 0 && p->op == SQLITE_INDEX_CONSTRAINT_EQ) {
      /* We have an equality constraint on the rowid. Use strategy 1. */
      int jj;
      for (jj = 0; jj < ii; jj++){
        pIdxInfo->aConstraintUsage[jj].argvIndex = 0;
        pIdxInfo->aConstraintUsage[jj].omit = 0;
      }
      pIdxInfo->idxNum = 1;
      pIdxInfo->aConstraintUsage[ii].argvIndex = 1;
      pIdxInfo->aConstraintUsage[ii].omit = 1; /* don't double check */

      /* This strategy involves a two rowid lookups on an B-Tree structures
      ** and then a linear search of an RD-Tree node. This should be 
      ** considered almost as quick as a direct rowid lookup (for which 
      ** sqlite uses an internal cost of 0.0).
      */ 
      pIdxInfo->estimatedCost = 10.0;
      return SQLITE_OK;
    }
    else if (p->op == SQLITE_INDEX_CONSTRAINT_MATCH) {
      /* We have a match constraint. Use strategy 2.
      */
      pIdxInfo->aConstraintUsage[ii].argvIndex = ii + 1;
      pIdxInfo->aConstraintUsage[ii].omit = 1;
    }
  }

  pIdxInfo->idxNum = 2;
  pIdxInfo->estimatedCost = (2000000.0 / (double)(pIdxInfo->nConstraint + 1));
  return rc;
}

/*
**
*/
static double itemWeightDistance(RDtreeItem *aItem, RDtreeItem *bItem)
{
  int d1 = abs(aItem->iMinWeight - bItem->iMinWeight);
  int d2 = abs(aItem->iMaxWeight - bItem->iMaxWeight);
  return (double) (d1 + d2);
  /* return (double) (d1 > d2) ? d1 : d2; */
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
    int iItem;
    sqlite3_int64 iBest = 0;

    int iMinGrowth = 0;
    double dMinDistance = 0.;
    int iMinWeight = 0;
    
    int nItem = NITEM(pNode);
    RDtreeItem item;
    RDtreeNode *pChild;

    /* Select the child node which will be enlarged the least if pItem
    ** is inserted into it.
    */
    for (iItem = 0; iItem < nItem; iItem++) {
      
      nodeGetItem(pRDtree, pNode, iItem, &item);
      
      int growth = itemGrowth(pRDtree, &item, pItem);
      double distance = itemWeightDistance(&item, pItem);
      int weight = itemWeight(pRDtree, &item);
      
      if (iItem == 0 ||
	  growth < iMinGrowth ||
	  (growth == iMinGrowth && distance < dMinDistance) ||
	  (growth == iMinGrowth && distance == dMinDistance
	   && weight < iMinWeight) 
	  ) {
        iMinGrowth = growth;
	dMinDistance = distance;
	iMinWeight = weight;
        iBest = item.iRowid;
      }
    }

    rc = nodeAcquire(pRDtree, iBest, pNode, &pChild);
    nodeRelease(pRDtree, pNode);
    pNode = pChild;
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
    if (!itemContains(pRDtree, &item, pItem)) {
      itemExtendBounds(pRDtree, &item, pItem);
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
** Pick the next item to be inserted into one of the two subsets. Select the
** one associated to a strongest "preference" for one of the two.
*/
static void pickNext(RDtree *pRDtree,
		     RDtreeItem *aItem, int nItem, int *aiUsed,
		     RDtreeItem *pLeftSeed, RDtreeItem *pRightSeed,
		     RDtreeItem *pLeftBounds, RDtreeItem *pRightBounds,
		     RDtreeItem **ppNext, int *pPreferRight)
{
  int iSelect = -1;
  int preferRight = 0;
  double dMaxPreference = -1.;
  int ii;
  for(ii = 0; ii < nItem; ii++){
    if( aiUsed[ii]==0 ){
      double left 
	= 1. - bfp_op_tanimoto(pRDtree->iBfpSize, 
			       aItem[ii].aBfp, pLeftSeed->aBfp);
      double right 
	= 1. - bfp_op_tanimoto(pRDtree->iBfpSize, 
			       aItem[ii].aBfp, pRightSeed->aBfp);
      double diff = left - right;
      double preference = 0.;
      if ((left + right) > 0.) {
	preference = abs(diff)/(left + right);
      }
      if (iSelect < 0 || preference > dMaxPreference) {
        dMaxPreference = preference;
        iSelect = ii;
	preferRight = diff > 0.;
      }
    }
  }
  aiUsed[iSelect] = 1;
  *ppNext = &aItem[iSelect];
  *pPreferRight = preferRight;
}

/*
** Pick the two most dissimilar fingerprints.
*/
static void pickSeeds(RDtree *pRDtree, RDtreeItem *aItem, int nItem, 
		      int *piLeftSeed, int *piRightSeed)
{
  int ii;
  int jj;

  int iLeftSeed = 0;
  int iRightSeed = 1;
  double dMaxDistance = 0.;

  for (ii = 0; ii < nItem; ii++) {
    for (jj = ii + 1; jj < nItem; jj++) {
      double tanimoto 
	= bfp_op_tanimoto(pRDtree->iBfpSize, aItem[ii].aBfp, aItem[jj].aBfp);
      double distance = 1. - tanimoto;

      if (distance > dMaxDistance) {
        iLeftSeed = ii;
        iRightSeed = jj;
        dMaxDistance = distance;
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
    int iPreferRight;
    RDtreeItem *pNext;
    pickNext(pRDtree, aItem, nItem, aiUsed, 
	     &aItem[iLeftSeed], &aItem[iRightSeed], pLeftBounds, pRightBounds,
	     &pNext, &iPreferRight);

    if ((RDTREE_MINITEMS(pRDtree) - NITEM(pRight) == i) ||
	(iPreferRight > 0 && (RDTREE_MINITEMS(pRDtree) - NITEM(pLeft) != i))) {
      nodeInsertItem(pRDtree, pRight, pNext);
      itemExtendBounds(pRDtree, pRightBounds, pNext);
    }
    else {
      nodeInsertItem(pRDtree, pLeft, pNext);
      itemExtendBounds(pRDtree, pLeftBounds, pNext);
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

  RDtreeNode *pLeft = 0;
  RDtreeNode *pRight = 0;

  RDtreeItem leftbounds;
  RDtreeItem rightbounds;

  /* Allocate an array and populate it with a copy of pItem and 
  ** all items from node pLeft. Then zero the original node.
  */
  aItem = sqlite3_malloc(sizeof(RDtreeItem) * (nItem + 1));
  if (!aItem) {
    rc = SQLITE_NOMEM;
    goto splitnode_out;
  }
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
    nodeReference(pLeft);
    pRight = nodeNew(pRDtree, pLeft->pParent);
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

/*
** deleting an Item may result in the removal of an underfull node from the
** that in turn requires the deletion of the corresponding Item from the
** parent node, and may therefore trigger the further removal of additional
** nodes.. removed nodes are collected into a linked list where they are 
** staged for later reinsertion of their items into the tree.
*/

static int removeNode(RDtree *pRDtree, RDtreeNode *pNode, int iHeight)
{
  int rc;
  int rc2;
  RDtreeNode *pParent = 0;
  int iItem;

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
      itemExtendBounds(pRDtree, &bounds, &item);
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
  RDtreeItem item;                /* New item to insert if argc>1 */
  int bHaveRowid = 0;             /* Set to 1 after new rowid is determined */

  rdtreeReference(pRDtree);
  assert(argc == 1 || argc == 4);

  /* Constraint handling. A write operation on an rd-tree table may return
  ** SQLITE_CONSTRAINT in case of a duplicate rowid value or in case the 
  ** argument type doesn't correspond to a binary fingerprint.
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
    if (sqlite3_value_type(argv[2]) != SQLITE_NULL ) {

      rowid = sqlite3_value_int64(argv[2]);

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

    if (sqlite3_value_type(argv[3]) != SQLITE_BLOB) {
      rc = SQLITE_MISMATCH;
    }
    else if (sqlite3_value_bytes(argv[3]) != pRDtree->iBfpSize) {
      rc = SQLITE_MISMATCH;
    }
    else {
      if (bHaveRowid) {
	item.iRowid = rowid;
      }
      memcpy(item.aBfp, sqlite3_value_blob(argv[3]), pRDtree->iBfpSize);
      item.iMinWeight = item.iMaxWeight = bfp_op_weight(pRDtree->iBfpSize, item.aBfp);
    }

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
      rc = newRowid(pRDtree, &item.iRowid);
    }
    *pRowid = item.iRowid;

    if (rc == SQLITE_OK) {
      rc = chooseLeaf(pRDtree, &item, 0, &pLeaf);
    }

    if (rc == SQLITE_OK) {
      int rc2;
      rc = rdtreeInsertItem(pRDtree, pLeaf, &item, 0);
      rc2 = nodeRelease(pRDtree, pLeaf);
      if (rc == SQLITE_OK) {
        rc = rc2;
      }
    }
  }

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
  rdtreeBestIndex,             /* xBestIndex - Determine search strategy */
  rdtreeDisconnect,            /* xDisconnect - Disconnect from a table */
  rdtreeDestroy,               /* xDestroy - Drop a table */
  rdtreeOpen,                  /* xOpen - open a cursor */
  rdtreeClose,                 /* xClose - close a cursor */
  rdtreeFilter,                /* xFilter - configure scan constraints */
  rdtreeNext,                  /* xNext - advance a cursor */
  rdtreeEof,                   /* xEof */
  rdtreeColumn,                /* xColumn - read data */
  rdtreeRowid,                 /* xRowid - read data */
  rdtreeUpdate,                /* xUpdate - write data */
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

  pRDtree->iNodeCapacity = (pRDtree->iNodeSize - 4)/pRDtree->nBytesPerItem;

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

  int iBfpSize = MOL_SIGNATURE_SIZE;  /* Default size of binary fingerprint */

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

  sqlite3_vtab_config(db, SQLITE_VTAB_CONSTRAINT_SUPPORT, 1);

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
  pRDtree->nBytesPerItem = 8 /* row id */ + 4 /* min/max weight */ + iBfpSize; 
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

/**
*** Subset match operator
**/

/*
** xTestInternal/xTestLeaf implementation for subset search/filtering
** same test is used for both internal and leaf nodes.
**
** If the item doesn't contain the constraint's bfp, then it's discarded (for
** internal nodes it means that if the bfp is not in the union of the child 
** nodes then it's in none of them). 
*/
static int subsetTest(RDtree* pRDtree, 
		      RDtreeConstraint* pCons, RDtreeItem* pItem, int* pEof)
{
  if (pItem->iMaxWeight < pCons->iWeight) {
    *pEof = 1;
  }
  else {
    *pEof = bfp_op_contains(pRDtree->iBfpSize, pItem->aBfp, pCons->aBfp) ? 0 : 1;
  }
  return SQLITE_OK;
}

static RDtreeMatchOp subsetMatchOp = { subsetTest, subsetTest };

/*
** A factory function for a substructure search match object
*/
static void rdtree_subset_f(sqlite3_context* ctx, 
			    int argc, sqlite3_value** argv)
{
  assert(argc == 1);

  int sz;
  RDtreeMatchArg *pMatchArg;
  int rc = SQLITE_OK;

  /* Check that value is a blob */
  if (sqlite3_value_type(argv[0]) != SQLITE_BLOB) {
    rc = SQLITE_MISMATCH;
  }
  /* Check that the blob is not bigger than the max allowed bfp */
  else if ((sz = sqlite3_value_bytes(argv[0])) > MAX_BITSTRING_SIZE) {
    rc = SQLITE_TOOBIG;
  }
  else if (!(pMatchArg = 
	     (RDtreeMatchArg *)sqlite3_malloc(sizeof(RDtreeMatchArg)))) {
    rc = SQLITE_NOMEM;
  }
  else {
    pMatchArg->magic = RDTREE_MATCH_MAGIC;
    pMatchArg->constraint.op = &subsetMatchOp;
    memcpy(pMatchArg->constraint.aBfp, sqlite3_value_blob(argv[0]), sz);
    pMatchArg->constraint.iWeight = bfp_op_weight(sz, pMatchArg->constraint.aBfp);
  }

  if (rc == SQLITE_OK) {
    sqlite3_result_blob(ctx, pMatchArg, sizeof(RDtreeMatchArg), sqlite3_free);
  }	
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}


/**
*** Tanimoto similarity match operator
**/

/*
** xTestInternal/xTestLeaf implementation for tanimoto similarity 
** search/filtering
*/
static int tanimotoTestInternal(RDtree* pRDtree, 
				RDtreeConstraint* pCons, RDtreeItem* pItem, 
				int* pEof)
{
  if ((pCons->dParam > 0.) &&
      ((pCons->iWeight*pCons->dParam > pItem->iMaxWeight) || (pCons->iWeight/pCons->dParam < pItem->iMinWeight))) {
    /* It is known that for the tanimoto similarity to be above a given 
    ** threshold t, it must be
    **
    ** Na*t <= Nb <= Na/t
    **
    ** And for the fingerprints in pItem we have that 
    ** 
    ** iMinWeight <= Nb <= iMaxWeight
    **
    ** so if (Na*t > iMaxWeight) or (Na/t < iMinWeight) this item can be discarded.
    */
    *pEof = 1;
  }  
  else {
    /* The item in the internal node stores the union of the fingerprints 
    ** that populate the child nodes. I want to use this union to compute an
    ** upper bound to the similarity. If this upper bound is lower than the 
    ** threashold value then the item can be discarded and the referred branch
    ** pruned.
    **
    ** T = Nsame / (Na + Nb - Nsame) <= Nsame / Na
    */
    int iweight = bfp_op_iweight(pRDtree->iBfpSize, pItem->aBfp, pCons->aBfp);
    if (pCons->iWeight &&
	((double)iweight)/pCons->iWeight >= pCons->dParam) {
      *pEof = 0;
    }
    else {
      *pEof = 1;
    }
  }
  return SQLITE_OK;
}

static int tanimotoTestLeaf(RDtree* pRDtree, 
			    RDtreeConstraint* pCons, RDtreeItem* pItem, 
			    int* pEof)
{
  int weight = pItem->iMaxWeight; /* on a leaf node */
  if ((pCons->dParam > 0.) &&
      ((pCons->iWeight*pCons->dParam > weight) || (pCons->iWeight/pCons->dParam < weight))) {
    /* skip the similarity computation when possible, 
    ** see comment in function above testing the internal node 
    */
    *pEof = 1;
    return SQLITE_OK;
  }

  int iweight = bfp_op_iweight(pRDtree->iBfpSize, pItem->aBfp, pCons->aBfp);
  int uweight = pItem->iMaxWeight + pCons->iWeight - iweight;
  double similarity = uweight ? ((double)iweight)/uweight : 1.;

  *pEof = similarity >= pCons->dParam ? 0 : 1;
  
  return SQLITE_OK;
}

static RDtreeMatchOp tanimotoMatchOp = { 
  tanimotoTestInternal, 
  tanimotoTestLeaf
};

/*
** A factory function for a tanimoto similarity search match object
*/
static void rdtree_tanimoto_f(sqlite3_context* ctx, 
			      int argc, sqlite3_value** argv)
{
  assert(argc == 2);

  int sz;
  RDtreeMatchArg *pMatchArg;
  int rc = SQLITE_OK;

  /* Check that the first argument is a blob */
  if (sqlite3_value_type(argv[0]) != SQLITE_BLOB) {
    rc = SQLITE_MISMATCH;
  }
  /* Check that the blob is not bigger than the max allowed bfp */
  else if ((sz = sqlite3_value_bytes(argv[0])) > MAX_BITSTRING_SIZE) {
    rc = SQLITE_TOOBIG;
  }
  /* Check that the second argument is a float number */
  else if (sqlite3_value_type(argv[1]) != SQLITE_FLOAT) {
    rc = SQLITE_MISMATCH;
  }
  else if (!(pMatchArg = 
	     (RDtreeMatchArg *)sqlite3_malloc(sizeof(RDtreeMatchArg)))) {
    rc = SQLITE_NOMEM;
  }
  else {
    pMatchArg->magic = RDTREE_MATCH_MAGIC;
    pMatchArg->constraint.op = &tanimotoMatchOp;
    memcpy(pMatchArg->constraint.aBfp, sqlite3_value_blob(argv[0]), sz);
    pMatchArg->constraint.iWeight = bfp_op_weight(sz, pMatchArg->constraint.aBfp);
    pMatchArg->constraint.dParam = sqlite3_value_double(argv[1]);
  }

  if (rc == SQLITE_OK) {
    sqlite3_result_blob(ctx, pMatchArg, sizeof(RDtreeMatchArg), sqlite3_free);
  }	
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

/**
*** Module init
**/

int chemicalite_init_rdtree(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) {
    rc = sqlite3_create_module_v2(db, "rdtree", &rdtreeModule, 
				  0,  /* Client data for xCreate/xConnect */
				  0   /* Module destructor function */
				  );
  }

  CREATE_SQLITE_UNARY_FUNCTION(rdtree_subset, rc);
  CREATE_SQLITE_BINARY_FUNCTION(rdtree_tanimoto, rc);

  return rc;
}
