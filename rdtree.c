#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "chemicalite.h"
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
  return (p[0]<<8) + p[1];
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

/* 
** Structure to store a deserialized rd-tree record.
*/
struct RDtreeItem {
  i64 iRowid;
  u8 aBfp[];
};

#define RDTREE_MAXITEMS 8

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
  assert( pNode->pNext==0 );
  int iHash = nodeHash(pNode->iNode);
  pNode->pNext = pRDtree->aHash[iHash];
  pRDtree->aHash[iHash] = pNode;
}

/*
** Remove node pNode from the node hash table.
*/
static void nodeHashDelete(RDtree *pRDtree, RDtreeNode *pNode)
{
  if (pNode->iNode != 0) { /* FIXME assert( pNode->pNext !=0 ) ? */
    RDtreeNode **pp = &pRDtree->aHash[nodeHash(pNode->iNode)];
    while ((*pp) != pNode) {
      pp = &(*pp)->pNext; 
      assert(*pp); 
    }
    *pp = pNode->pNext;
    pNode->pNext = 0;
  }
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
  0, /* rdtreeRename,                /* xRename - rename the table */
  0,                           /* xSavepoint */
  0,                           /* xRelease */
  0                            /* xRollbackTo */
};

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
      if ((4 + sizeof(RDtreeItem) + pRDtree->iBfpSize) < pRDtree->iNodeSize ) {
        pRDtree->iNodeSize = 4 + sizeof(RDtreeItem) + pRDtree->iBfpSize;
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

  int iBfpSize = 128;   /* Default size of binary fingerprint FIXME */

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
