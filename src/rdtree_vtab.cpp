#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

#include "rdtree_vtab.hpp"
#include "rdtree_strategy.hpp"
#include "rdtree_node.hpp"
#include "rdtree_item.hpp"
#include "rdtree_cursor.hpp"

#include "bfp.hpp"
#include "bfp_ops.hpp"

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
*/

static const int RDTREE_MAX_BITSTRING_SIZE = 256;
static const int RDTREE_MAXITEMS = 51;

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
static const int RDTREE_MAX_DEPTH = 64;

static const unsigned int RDTREE_FLAGS_UNASSIGNED = 0;
static const unsigned int RDTREE_OPTIMIZED_FOR_SUBSET_QUERIES = 1;
static const unsigned int RDTREE_OPTIMIZED_FOR_SIMILARITY_QUERIES = 2;

int RDtreeVtab::create(
  sqlite3 *db, void */*paux*/, int argc, const char *const*argv, 
  sqlite3_vtab **pvtab, char **err)
{
  return init(db, argc, argv, pvtab, err, 1);
}

int RDtreeVtab::connect(
  sqlite3 *db, void */*paux*/, int argc, const char *const*argv, 
  sqlite3_vtab **pvtab, char **err)
{
  return init(db, argc, argv, pvtab, err, 0);
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
int RDtreeVtab::init(
  sqlite3 *db, int argc, const char *const*argv, 
  sqlite3_vtab **pvtab, char **err, int is_create)
{
  /* perform arg checking */
  if (argc < 5) {
    *err = sqlite3_mprintf("wrong number of arguments. "
                             "two column definitions are required.");
    return SQLITE_ERROR;
  }
  if (argc > 6) {
    *err = sqlite3_mprintf("wrong number of arguments. "
                             "at most one optional argument is expected.");
    return SQLITE_ERROR;
  }

  int bfp_bytes; /* Length (in bytes) of stored binary fingerprint */

  int bfp_size_arg;
  if (sscanf(argv[4], "%*s bits( %d )", &bfp_size_arg) == 1) {
      if (bfp_size_arg <= 0 || bfp_size_arg % 8) {
        *err = sqlite3_mprintf("invalid number of bits for a stored fingerprint: '%d'", bfp_size_arg);
        return SQLITE_ERROR;
      }
      bfp_bytes = bfp_size_arg/8;
  }
  else if (sscanf(argv[4], "%*s bytes( %d )", &bfp_size_arg) == 1) {
      if (bfp_size_arg <= 0) {
        *err = sqlite3_mprintf("invalid number of bytes for a stored fingerprint: '%d'", bfp_size_arg);
        return SQLITE_ERROR;
      }
      bfp_bytes = bfp_size_arg;
  }
  else {
    *err = sqlite3_mprintf("unable to parse the fingerprint size from: '%s'", argv[4]);
    return SQLITE_ERROR;
  }

  if (bfp_bytes > RDTREE_MAX_BITSTRING_SIZE) {
    *err = sqlite3_mprintf("the requested fingerpring size exceeds the supported max value: %d bytes", RDTREE_MAX_BITSTRING_SIZE);
    return SQLITE_ERROR;
  }

  unsigned int flags = RDTREE_FLAGS_UNASSIGNED;
  if (argc == 6) {
    if (strcmp(argv[5], "OPTIMIZED_FOR_SUBSET_QUERIES") == 0) {
      flags |= RDTREE_OPTIMIZED_FOR_SUBSET_QUERIES;
    }
    else if (strcmp(argv[5], "OPTIMIZED_FOR_SIMILARITY_QUERIES") == 0) {
      flags |= RDTREE_OPTIMIZED_FOR_SIMILARITY_QUERIES;
    }
    else {
      *err = sqlite3_mprintf("unrecognized option: %s", argv[5]);
      return SQLITE_ERROR;
    }
  }

  sqlite3_vtab_config(db, SQLITE_VTAB_CONSTRAINT_SUPPORT, 1);

  /* Allocate the sqlite3_vtab structure */
  RDtreeVtab * rdtree = new RDtreeVtab; // FIXME try/catch?
  rdtree->db_name = argv[1];
  rdtree->table_name = argv[2];
  rdtree->db = db;
  rdtree->bfp_bytes = bfp_bytes;
  rdtree->item_bytes = 8 /* row id */ + 4 /* min/max weight */ + bfp_bytes; 
  rdtree->n_ref = 1;
  
  if (flags | RDTREE_OPTIMIZED_FOR_SIMILARITY_QUERIES) {
    rdtree->strategy.reset(new RDtreeStrategySimilarity(rdtree));
  }
  else if (flags | RDTREE_OPTIMIZED_FOR_SUBSET_QUERIES) {
    rdtree->strategy.reset(new RDtreeStrategySubset(rdtree));
  }
  else {
    rdtree->strategy.reset(new RDtreeStrategyGeneric(rdtree));
  }

  /* Figure out the node size to use. */
  int rc = rdtree->get_node_bytes(is_create);

  /* Create/Connect to the underlying relational database schema. If
  ** that is successful, call sqlite3_declare_vtab() to configure
  ** the rd-tree table schema.
  */
  if (rc == SQLITE_OK) {
    if ((rc = rdtree->sql_init(is_create))) {
      *err = sqlite3_mprintf("%s", sqlite3_errmsg(db));
    } 
    else {
      char *sql = sqlite3_mprintf("CREATE TABLE x(%s", argv[3]);
      char *tmp;
      /* the current implementation requires 2 columns specs, plus
      ** an optional flag. in practice, the following loop will always
      ** execute one single iteration, but I'm leaving it here although
      ** more generic than needed, just in case it may be useful again at
      ** some point in the future
      */
      for (int ii=4; sql && ii<5; ++ii) {
        tmp = sql;
        sql = sqlite3_mprintf("%s, %s", tmp, argv[ii]);
        sqlite3_free(tmp);
      }
      if (sql) {
        tmp = sql;
        sql = sqlite3_mprintf("%s);", tmp);
        sqlite3_free(tmp);
      }
      if (!sql) {
        rc = SQLITE_NOMEM;
      }
      else if (SQLITE_OK!=(rc = sqlite3_declare_vtab(db, sql))) {
        *err = sqlite3_mprintf("%s", sqlite3_errmsg(db));
      }
      sqlite3_free(sql);
    }
  }

  if (rc==SQLITE_OK) {
    *pvtab = (sqlite3_vtab *)rdtree;
  }
  else {
    rdtree->decref();
  }

  return rc;
}

static int select_int(sqlite3 * db, const char *query, int *value)
{
  int rc = SQLITE_OK;
  sqlite3_stmt *stmt;

  rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    return rc;
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    return SQLITE_ERROR;
  }

  int value_type = sqlite3_column_type(stmt, 0);
  if (value_type != SQLITE_INTEGER) {
    return SQLITE_MISMATCH;
  }

  *value = sqlite3_column_int(stmt, 0);

  return sqlite3_finalize(stmt);
}

/*
** This function is called from within the xConnect() or xCreate() method to
** determine the node-size used by the rdtree table being created or connected
** to. If successful, pRDtree->node_bytes is populated and SQLITE_OK returned.
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
int RDtreeVtab::get_node_bytes(int is_create)
{
  int rc;
  char *sql;
  if (is_create) {
    int page_size = 0;
    sql = sqlite3_mprintf("PRAGMA %Q.page_size", db_name.c_str());
    rc = select_int(db, sql, &page_size);
    if (rc==SQLITE_OK) {
      node_bytes = page_size - 64;
      if ((4 + item_bytes*RDTREE_MAXITEMS) < node_bytes) {
        node_bytes = 4 + item_bytes*RDTREE_MAXITEMS;
      }
    }
  }
  else{
    sql = sqlite3_mprintf("SELECT length(data) FROM '%q'.'%q_node' "
			   "WHERE nodeno=1", db_name.c_str(), table_name.c_str());
    rc = select_int(db, sql, &node_bytes);
  }

  node_capacity = (node_bytes - 4)/item_bytes;

  sqlite3_free(sql);
  return rc;
}

int RDtreeVtab::sql_init(int is_create)
{
  int rc = SQLITE_OK;

  if (is_create) {
    char *create 
      = sqlite3_mprintf("CREATE TABLE \"%w\".\"%w_node\"(nodeno INTEGER PRIMARY KEY, data BLOB);"
			"CREATE TABLE \"%w\".\"%w_rowid\"(rowid INTEGER PRIMARY KEY, nodeno INTEGER);"
			"CREATE TABLE \"%w\".\"%w_parent\"(nodeno INTEGER PRIMARY KEY, parentnode INTEGER);"
			"CREATE TABLE \"%w\".\"%w_bitfreq\"(bitno INTEGER PRIMARY KEY, freq INTEGER);"
			"CREATE TABLE \"%w\".\"%w_weightfreq\"(weight INTEGER PRIMARY KEY, freq INTEGER);"
			"INSERT INTO \"%w\".\"%w_node\" VALUES(1, zeroblob(%d))",
			db_name.c_str(), table_name.c_str(), 
			db_name.c_str(), table_name.c_str(), 
			db_name.c_str(), table_name.c_str(), 
			db_name.c_str(), table_name.c_str(), 
			db_name.c_str(), table_name.c_str(), 
			db_name.c_str(), table_name.c_str(), node_bytes
			);
    if (!create) {
      return SQLITE_NOMEM;
    }
    rc = sqlite3_exec(db, create, 0, 0, 0);
    sqlite3_free(create);
    if (rc != SQLITE_OK) {
      return rc;
    }
    
    char *init_bitfreq
      = sqlite3_mprintf("INSERT INTO \"%w\".\"%w_bitfreq\" VALUES(?, 0)",
			db_name.c_str(), table_name.c_str()
			);
    if (!init_bitfreq) {
      return SQLITE_NOMEM;
    }
    sqlite3_stmt * init_bitfreq_stmt = 0;
    rc = sqlite3_prepare_v2(db, init_bitfreq, -1, &init_bitfreq_stmt, 0);
    sqlite3_free(init_bitfreq);
    if (rc != SQLITE_OK) {
      return rc;
    }

    for (int i=0; i < bfp_bytes*8; ++i) {
      rc = sqlite3_bind_int(init_bitfreq_stmt, 1, i);
      if (rc != SQLITE_OK) {
        break;
      }
      rc = sqlite3_step(init_bitfreq_stmt);
      if (rc != SQLITE_DONE) {
        break;
      }
      else {
        /* reassign the rc status and keep the error handling simple */
        rc = SQLITE_OK; 
      }
      sqlite3_reset(init_bitfreq_stmt);
    }
    sqlite3_finalize(init_bitfreq_stmt);
    if (rc != SQLITE_OK) {
      return rc;
    }

    char *init_weightfreq
      = sqlite3_mprintf("INSERT INTO \"%w\".\"%w_weightfreq\" VALUES(?, 0)",
			db_name.c_str(), table_name.c_str()
			);
    if (!init_weightfreq) {
      return SQLITE_NOMEM;
    }
    sqlite3_stmt * init_weightfreq_stmt = 0;
    rc = sqlite3_prepare_v2(db, init_weightfreq, -1, &init_weightfreq_stmt, 0);
    sqlite3_free(init_weightfreq);
    if (rc != SQLITE_OK) {
      return rc;
    }

    for (int i=0; i <= bfp_bytes*8; ++i) {
      rc = sqlite3_bind_int(init_weightfreq_stmt, 1, i);
      if (rc != SQLITE_OK) {
        break;
      }
      rc = sqlite3_step(init_weightfreq_stmt);
      
      if (rc != SQLITE_DONE) {
        break;
      }
      else {
        /* reassign the rc status and keep the error handling simple */
        rc = SQLITE_OK; 
      }
      sqlite3_reset(init_weightfreq_stmt);
    }
    sqlite3_finalize(init_weightfreq_stmt);
    if (rc != SQLITE_OK) {
      return rc;
    }
  }
  
  static constexpr const int N_STATEMENT = 13;

  static const char *asql[N_STATEMENT] = {
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
    "DELETE FROM '%q'.'%q_parent' WHERE nodeno = :1",

    /* Update the xxx_bitfreq table */
    "UPDATE '%q'.'%q_bitfreq' SET freq = freq + 1 WHERE bitno = :1",
    "UPDATE '%q'.'%q_bitfreq' SET freq = freq - 1 WHERE bitno = :1",

    /* Update the xxx_weightfreq table */
    "UPDATE '%q'.'%q_weightfreq' SET freq = freq + 1 WHERE weight = :1",
    "UPDATE '%q'.'%q_weightfreq' SET freq = freq - 1 WHERE weight = :1"
  };

  sqlite3_stmt **apstmt[N_STATEMENT] = {
    &pReadNode,
    &pWriteNode,
    &pDeleteNode,
    &pReadRowid,
    &pWriteRowid,
    &pDeleteRowid,
    &pReadParent,
    &pWriteParent,
    &pDeleteParent,
    &pIncrementBitfreq,
    &pDecrementBitfreq,
    &pIncrementWeightfreq,
    &pDecrementWeightfreq
  };

  for (int i=0; i<N_STATEMENT && rc==SQLITE_OK; i++) {
    char *sql = sqlite3_mprintf(asql[i], db_name.c_str(), table_name.c_str());
    if (sql) {
      rc = sqlite3_prepare_v3(db, sql, -1, SQLITE_PREPARE_PERSISTENT, apstmt[i], 0);
    }
    else {
      rc = SQLITE_NOMEM;
    }
    sqlite3_free(sql);
  }

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
int RDtreeVtab::bestindex(sqlite3_index_info *idxinfo)
{
  int rc = SQLITE_OK;
  bool match = false; /* True if there exists a MATCH constraint */
  int iIdx = 0;  /* argv index/counter */

  assert(idxinfo->idxStr==0);

  /* The comment below directly from SQLite's rtree extension */
  /* Check if there exists a MATCH constraint - even an unusable one. If there
  ** is, do not consider the lookup-by-rowid plan as using such a plan would
  ** require the VDBE to evaluate the MATCH constraint, which is not currently
  ** possible. */
  for (int ii=0; ii<idxinfo->nConstraint; ii++) {
    if (idxinfo->aConstraint[ii].op==SQLITE_INDEX_CONSTRAINT_MATCH) {
      match = true;
    }
  }

  for (int ii = 0; ii < idxinfo->nConstraint; ii++) {

    sqlite3_index_info::sqlite3_index_constraint *p = &idxinfo->aConstraint[ii];

    if (!p->usable) {
      continue;
    }

    if (!match && p->iColumn == 0 && p->op == SQLITE_INDEX_CONSTRAINT_EQ) {
      /* We have an equality constraint on the rowid. Use strategy 1. */
      for (int jj = 0; jj < ii; jj++){
        idxinfo->aConstraintUsage[jj].argvIndex = 0;
        idxinfo->aConstraintUsage[jj].omit = 0;
      }
      idxinfo->idxNum = 1;
      idxinfo->aConstraintUsage[ii].argvIndex = 1;
      idxinfo->aConstraintUsage[ii].omit = 1; /* don't double check */

      /* This strategy involves a two rowid lookups on an B-Tree structures
      ** and then a linear search of an RD-Tree node. This should be 
      ** considered almost as quick as a direct rowid lookup (for which 
      ** sqlite uses an internal cost of 0.0). It is expected to return
      ** a single row.
      */ 
      idxinfo->estimatedCost = 30.0;
      idxinfo->estimatedRows = 1;
      idxinfo->idxFlags = SQLITE_INDEX_SCAN_UNIQUE;
      return SQLITE_OK;
    }

    if (p->op == SQLITE_INDEX_CONSTRAINT_MATCH) {
      idxinfo->aConstraintUsage[ii].argvIndex = ++iIdx;
      idxinfo->aConstraintUsage[ii].omit = 1;
    }
  }

  idxinfo->idxNum = 2;
  idxinfo->estimatedCost = (2000000.0 / (double)(idxinfo->nConstraint + 1));
  /* TODO add estimated rows
  nRow = pRtree->nRowEst >> (iIdx/2);
  idxinfo->estimatedCost = (double)6.0 * (double)nRow;
  idxinfo->estimatedRows = nRow;
  */
  return rc;
}

/* 
** This function is the implementation of the xDestroy
** method of the rd-tree virtual table.
*/
int RDtreeVtab::destroy()
{
  int rc = SQLITE_OK;
 
  char *sql = sqlite3_mprintf("DROP TABLE '%q'.'%q_node';"
	"DROP TABLE '%q'.'%q_rowid';"
	"DROP TABLE '%q'.'%q_parent';"
	"DROP TABLE '%q'.'%q_bitfreq';"
	"DROP TABLE '%q'.'%q_weightfreq';",
	db_name.c_str(), table_name.c_str(), 
	db_name.c_str(), table_name.c_str(),
	db_name.c_str(), table_name.c_str(),
	db_name.c_str(), table_name.c_str(),
	db_name.c_str(), table_name.c_str());

  if (!sql) {
    rc = SQLITE_NOMEM;
  }
  else{
    rc = sqlite3_exec(db, sql, 0, 0, 0);
    sqlite3_free(sql);
  }

  if (rc == SQLITE_OK) {
    decref();
  }

  return rc;
}

/* 
** This function is the implementation of the xDisconnect
** method of the rd-tree virtual table.
*/
int RDtreeVtab::disconnect()
{
  decref();
  return SQLITE_OK;
}

/*
** Increment the bits frequency count
*/
int RDtreeVtab::increment_bitfreq(const uint8_t *bfp)
{
  int rc = SQLITE_OK;
  
  int i, bitno = 0;
  const uint8_t * bfp_end = bfp + bfp_bytes;
  
  while (bfp < bfp_end) {
    uint8_t byte = *bfp++;
    for (i = 0; i < 8; ++i, ++bitno, byte>>=1) {
      if (byte & 0x01) {
        sqlite3_bind_int(pIncrementBitfreq, 1, bitno);
        sqlite3_step(pIncrementBitfreq);
        rc = sqlite3_reset(pIncrementBitfreq);
      }
    }
  }
  return rc;
}


/*
** Decrement the bits frequency count
*/
int RDtreeVtab::decrement_bitfreq(const uint8_t *bfp)
{
  int rc = SQLITE_OK;
  
  int i, bitno = 0;
  const uint8_t * bfp_end = bfp + bfp_bytes;
  
  while (bfp < bfp_end) {
    uint8_t byte = *bfp++;
    for (i = 0; i < 8; ++i, ++bitno, byte>>=1) {
      if (byte & 0x01) {
        sqlite3_bind_int(pDecrementBitfreq, 1, bitno);
	    sqlite3_step(pDecrementBitfreq);
	    rc = sqlite3_reset(pDecrementBitfreq);
      }
    }
  }
  return rc;
}


/*
** Increment the weight frequency count
*/
int RDtreeVtab::increment_weightfreq(int weight)
{
  int rc = SQLITE_OK;
  sqlite3_bind_int(pIncrementWeightfreq, 1, weight);
  sqlite3_step(pIncrementWeightfreq);
  rc = sqlite3_reset(pIncrementWeightfreq);
  return rc;
}


/*
** Decrement the weight frequency count
*/
int RDtreeVtab::decrement_weightfreq(int weight)
{
  int rc = SQLITE_OK;
  sqlite3_bind_int(pDecrementWeightfreq, 1, weight);
  sqlite3_step(pDecrementWeightfreq);
  rc = sqlite3_reset(pDecrementWeightfreq);
  return rc;
}

/*
** Write mapping (iRowid->iNode) to the <rdtree>_rowid table.
*/
int RDtreeVtab::rowid_write(sqlite3_int64 rowid, sqlite3_int64 nodeid)
{
  sqlite3_bind_int64(pWriteRowid, 1, rowid);
  sqlite3_bind_int64(pWriteRowid, 2, nodeid);
  sqlite3_step(pWriteRowid);
  return sqlite3_reset(pWriteRowid);
}

/*
** Write mapping (iNode->iPar) to the <rdtree>_parent table.
*/
int RDtreeVtab::parent_write(sqlite3_int64 nodeid, sqlite3_int64 parentid)
{
  sqlite3_bind_int64(pWriteParent, 1, nodeid);
  sqlite3_bind_int64(pWriteParent, 2, parentid);
  sqlite3_step(pWriteParent);
  return sqlite3_reset(pWriteParent);
}

/*
** Allocate and return new rd-tree node. Initially, (RDtreeNode.iNode==0),
** indicating that node has not yet been assigned a node number. It is
** assigned a node number when nodeWrite() is called to write the
** node contents out to the database.
*/
RDtreeNode * RDtreeVtab::node_new(RDtreeNode *parent)
{
  RDtreeNode *node = new RDtreeNode(this, parent);
  node->dirty = true;
  node_incref(parent);
  return node;
}

/*
** Search the node hash table for node iNode. If found, return a pointer
** to it. Otherwise, return 0.
*/
RDtreeNode * RDtreeVtab::node_hash_lookup(sqlite3_int64 nodeid)
{
  RDtreeNode *p = nullptr;
  auto nit = node_hash.find(nodeid);
  if (nit != node_hash.end()) {
    p = nit->second;
  }
  return p;
}

/*
** Add node pNode to the node hash table.
*/
void RDtreeVtab::node_hash_insert(RDtreeNode *node)
{
  assert(node_hash.find(node->nodeid) == node_hash.end());
  node_hash[node->nodeid] = node;
}

/*
** Remove node pNode from the node hash table.
*/
void RDtreeVtab::node_hash_remove(RDtreeNode *node)
{
  if (node->nodeid != 0) {
    node_hash.erase(node->nodeid);
  }
}

/*
** Obtain a reference to an rd-tree node.
*/
int RDtreeVtab::node_acquire(
    sqlite3_int64 nodeid, RDtreeNode *parent, RDtreeNode **acquired)
{
  int rc;

  /* Check if the requested node is already in the hash table. If so,
  ** increase its reference count and return it.
  */
  RDtreeNode * node = node_hash_lookup(nodeid);
  if (node) {
    assert(!parent || !node->parent || node->parent == parent);
    if (parent && !node->parent) {
      node_incref(parent);
      node->parent = parent;
    }
    node_incref(node);
    *acquired = node;
    return SQLITE_OK;
  }

  sqlite3_bind_int64(pReadNode, 1, nodeid);
  rc = sqlite3_step(pReadNode);

  if (rc == SQLITE_ROW) {
    const uint8_t *blob = (const uint8_t *)sqlite3_column_blob(pReadNode, 0);
    if (node_bytes == sqlite3_column_bytes(pReadNode, 0)) {
      node = new RDtreeNode(this, parent);
      node->nodeid = nodeid;
      memcpy(node->data.data(), blob, node_bytes);
      node_incref(parent);
    }
  }

  rc = sqlite3_reset(pReadNode);

  /* If the root node was just loaded, set pRDtree->iDepth to the height
  ** of the rd-tree structure. A height of zero means all data is stored on
  ** the root node. A height of one means the children of the root node
  ** are the leaves, and so on. If the depth as specified on the root node
  ** is greater than RDTREE_MAX_DEPTH, the rd-tree structure must be corrupt.
  */
  if (node && nodeid == 1) {
    depth = read_uint16(node->data.data());
    if (depth > RDTREE_MAX_DEPTH) {
      rc = SQLITE_CORRUPT_VTAB;
    }
  }

  /* If no error has occurred so far, check if the "number of entries"
  ** field on the node is too large. If so, set the return code to 
  ** SQLITE_CORRUPT_VTAB.
  */
  if (node && rc == SQLITE_OK) {
    if (node->get_size() > node_capacity) {
      rc = SQLITE_CORRUPT_VTAB;
    }
  }
  
  if (rc == SQLITE_OK) {
    if (node) {
      node_hash_insert(node);
    }
    else {
      rc = SQLITE_CORRUPT_VTAB;
    }
    *acquired = node;
  }
  else {
    delete node;
    *acquired = nullptr;
  }

  return rc;
}

/*
** If the node is dirty, write it out to the database.
*/
int RDtreeVtab::node_write(RDtreeNode *node)
{
  int rc = SQLITE_OK;
  if (node->dirty) {
    if (node->nodeid) {
      sqlite3_bind_int64(pWriteNode, 1, node->nodeid);
    }
    else {
      sqlite3_bind_null(pWriteNode, 1);
    }
    sqlite3_bind_blob(pWriteNode, 2, node->data.data(), node_bytes, SQLITE_TRANSIENT);
    sqlite3_step(pWriteNode);
    node->dirty = false;
    rc = sqlite3_reset(pWriteNode);
    if (node->nodeid == 0 && rc == SQLITE_OK) {
      node->nodeid = sqlite3_last_insert_rowid(db);
      node_hash_insert(node);
    }
  }
  return rc;
}

/*
** Increase the reference count for a node.
*/
void RDtreeVtab::node_incref(RDtreeNode *node)
{
  if (node) {
    node->n_ref++;
  }
}

/*
** Decrease the reference count for a node. If the ref count drops to zero,
** release it.
*/
int RDtreeVtab::node_decref(RDtreeNode *node)
{
  int rc = SQLITE_OK;
  if (node) {
    assert(node->n_ref > 0);
    --node->n_ref;
    if (node->n_ref == 0) {
      rc = node_release(node);
    }
  }
  return rc;
}

/*
** Release a reference to a node. If the node is dirty and the reference
** count drops to zero, the node data is written to the database.
*/
int RDtreeVtab::node_release(RDtreeNode *node)
{
  int rc = SQLITE_OK;
  if (node->nodeid == 1) {
    depth = -1;
  }
  if (node->parent) {
    rc = node_decref(node->parent);
  }
  if (rc == SQLITE_OK) {
    rc = node_write(node);
  }
  node_hash_remove(node);
  delete node;
  return rc;
}

/* 
** Use node_acquire() to obtain the leaf node containing the record with 
** rowid iRowid. If successful, set *ppLeaf to point to the node and
** return SQLITE_OK. If there is no such record in the table, set
** *ppLeaf to 0 and return SQLITE_OK. If an error occurs, set *ppLeaf
** to zero and return an SQLite error code.
*/
int RDtreeVtab::find_leaf_node(sqlite3_int64 rowid, RDtreeNode **leaf)
{
  int rc = SQLITE_OK;
  *leaf = nullptr;
  sqlite3_bind_int64(pReadRowid, 1, rowid);
  if (sqlite3_step(pReadRowid) == SQLITE_ROW) {
    sqlite3_int64 nodeid = sqlite3_column_int64(pReadRowid, 0);
    rc = node_acquire(nodeid, 0, leaf);
  }
  int rc2 = sqlite3_reset(pReadRowid);
  if (rc == SQLITE_OK) {
    rc = rc2;
  }
  return rc;
}

/*
** An item with the same content as pItem has just been inserted into
** the node pNode. This function updates the bounds in
** all ancestor elements.
*/
int RDtreeVtab::adjust_tree(RDtreeNode *node, RDtreeItem *new_item)
{
  RDtreeNode *p = node;
  while (p->parent) {
    RDtreeNode *parent = p->parent;
    RDtreeItem item(bfp_bytes);
    int idx;

    if (p->get_index_in_parent(&idx)) {
      return SQLITE_CORRUPT_VTAB;
    }

    parent->get_item(idx, &item);
    if (!item.contains(*new_item)) {
      item.extend_bounds(*new_item);
      parent->overwrite_item(idx, &item);
    }
 
    p = parent;
  }
  return SQLITE_OK;
}

int RDtreeVtab::update_mapping(sqlite3_int64 rowid, RDtreeNode *node, int height)
{
  if (height > 0) {
    RDtreeNode *child = node_hash_lookup(rowid);
    if (child) {
      node_decref(child->parent);
      node_incref(node);
      child->parent = node;
    }
  }

  if (height == 0) {
    return rowid_write(rowid, node->nodeid);
  }
  else {
    return parent_write(rowid, node->nodeid);
  }
}

int RDtreeVtab::split_node(RDtreeNode *node, RDtreeItem *item, int height)
{
  int new_item_is_right = 0;

  int rc = SQLITE_OK;
  int node_size = node->get_size();

  RDtreeNode *left = 0;
  RDtreeNode *right = 0;

  RDtreeItem leftbounds(bfp_bytes);
  RDtreeItem rightbounds(bfp_bytes);

  /* Allocate an array and populate it with a copy of pItem and 
  ** all items from node left. Then zero the original node.
  */
  std::vector<RDtreeItem> items(node_size + 1, RDtreeItem(bfp_bytes)); // TODO: try/catch
  for (int i = 0; i < node_size; i++) {
    node->get_item(i, &items[i]);
  }
  node->zero();
  items[node_size] = *item;
  node_size += 1;

  if (node->nodeid == 1) { /* splitting the root node */
    right = node_new(node);
    left = node_new(node);
    depth++;
    node->dirty = true;
    write_uint16(node->data.data(), depth);
  }
  else {
    left = node;
    node_incref(left);
    right = node_new(left->parent);
  }

  if (!left || !right) {
    // FIXME (out of memory scenario)
    rc = SQLITE_NOMEM;
    node_decref(right);
    node_decref(left);
    return rc;
  }

  memset(left->data.data(), 0, node_bytes);
  memset(right->data.data(), 0, node_bytes);

  rc = strategy->assign_items(
    items.data(), node_size, left, right, &leftbounds, &rightbounds);

  if (rc != SQLITE_OK) {
    node_decref(right);
    node_decref(left);
    return rc;
  }

  /* Ensure both child nodes have node numbers assigned to them by calling
  ** nodeWrite(). Node right always needs a node number, as it was created
  ** by nodeNew() above. But node left sometimes already has a node number.
  ** In this case avoid the call to nodeWrite().
  */
  if ((SQLITE_OK != (rc = node_write(right))) || 
      (0 == left->nodeid && SQLITE_OK != (rc = node_write(left)))) {
    node_decref(right);
    node_decref(left);
    return rc;
  }

  rightbounds.rowid = right->nodeid;
  leftbounds.rowid = left->nodeid;

  if (node->nodeid == 1) {
    rc = insert_item(left->parent, &leftbounds, height+1);
    if (rc != SQLITE_OK) {
      node_decref(right);
      node_decref(left);
      return rc;
    }
  }
  else {
    RDtreeNode *parent = left->parent;
    int parent_idx;
    rc = left->get_index_in_parent(&parent_idx);
    if (rc == SQLITE_OK) {
      parent->overwrite_item(parent_idx, &leftbounds);
      rc = adjust_tree(parent, &leftbounds);
    }
    if (rc != SQLITE_OK) {
      node_decref(right);
      node_decref(left);
      return rc;
    }
  }

  if ((rc = 
       insert_item(right->parent, &rightbounds, height+1))) {
    node_decref(right);
    node_decref(left);
    return rc;
  }

  int right_size = right->get_size();
  for (int i = 0; i < right_size; i++) {
    sqlite3_int64 rowid = right->get_rowid(i);
    rc = update_mapping(rowid, right, height);
    if (rowid == item->rowid) {
      new_item_is_right = 1;
    }
    if (rc != SQLITE_OK) {
      node_decref(right);
      node_decref(left);
      return rc;
    }
  }

  if (node->nodeid == 1) {
    int left_size = left->get_size();
    for (int i = 0; i < left_size; i++) {
      sqlite3_int64 rowid = left->get_rowid(i);
      rc = update_mapping(rowid, left, height);
      if (rc != SQLITE_OK) {
        node_decref(right);
        node_decref(left);
        return rc;
      }
    }
  }
  else if (new_item_is_right == 0) {
    rc = update_mapping(item->rowid, left, height);
  }

  node_decref(right);
  node_decref(left);
  return rc;
}

/*
** If node pLeaf is not the root of the rd-tree and its parent pointer is 
** still NULL, load all ancestor nodes of pLeaf into memory and populate
** the pLeaf->parent chain all the way up to the root node.
**
** This operation is required when a row is deleted (or updated - an update
** is implemented as a delete followed by an insert). SQLite provides the
** rowid of the row to delete, which can be used to find the leaf on which
** the entry resides (argument pLeaf). Once the leaf is located, this 
** function is called to determine its ancestry.
*/
int RDtreeVtab::fix_leaf_parent(RDtreeNode *leaf)
{
  int rc = SQLITE_OK;
  RDtreeNode *child = leaf;
  while (rc == SQLITE_OK && child->nodeid != 1 && child->parent == 0) {
    int rc2 = SQLITE_OK;          /* sqlite3_reset() return code */
    sqlite3_bind_int64(pReadParent, 1, child->nodeid);
    rc = sqlite3_step(pReadParent);
    if (rc == SQLITE_ROW) {
      RDtreeNode *test;            /* Used to test for reference loops */
      sqlite3_int64 nodeid;        /* Node number of parent node */

      /* Before setting pChild->parent, test that we are not creating a
      ** loop of references (as we would if, say, pChild==parent). We don't
      ** want to do this as it leads to a memory leak when trying to delete
      ** the reference counted node structures.
      */
      nodeid = sqlite3_column_int64(pReadParent, 0);
      for (test = leaf; test && test->nodeid != nodeid; test = test->parent)
	      ; /* loop from pLeaf up towards pChild looking for nodeid.. */

      if (!test) { /* Ok */
        rc2 = node_acquire(nodeid, 0, &child->parent);
      }
    }
    rc = sqlite3_reset(pReadParent);
    if (rc == SQLITE_OK) rc = rc2;
    if (rc == SQLITE_OK && !child->parent) rc = SQLITE_CORRUPT_VTAB;
    child = child->parent;
  }
  return rc;
}

/*
** deleting an Item may result in the removal of an underfull node from the
** that in turn requires the deletion of the corresponding Item from the
** parent node, and may therefore trigger the further removal of additional
** nodes.. removed nodes are collected into a linked list where they are 
** staged for later reinsertion of their items into the tree.
*/

int RDtreeVtab::remove_node(RDtreeNode *node, int height)
{
  int rc;
  int rc2;
  RDtreeNode *parent = 0;
  int item;

  assert( node->n_ref == 1 );

  /* Remove the entry in the parent item. */
  rc = node->get_index_in_parent(&item);
  if (rc == SQLITE_OK) {
    parent = node->parent;
    node->parent = 0;
    rc = delete_item(parent, item, height+1);
  }
  rc2 = node_decref(parent);
  if (rc == SQLITE_OK) {
    rc = rc2;
  }
  if (rc != SQLITE_OK) {
    return rc;
  }

  /* Remove the xxx_node entry. */
  sqlite3_bind_int64(pDeleteNode, 1, node->nodeid);
  sqlite3_step(pDeleteNode);
  if (SQLITE_OK != (rc = sqlite3_reset(pDeleteNode))) {
    return rc;
  }

  /* Remove the xxx_parent entry. */
  sqlite3_bind_int64(pDeleteParent, 1, node->nodeid);
  sqlite3_step(pDeleteParent);
  if (SQLITE_OK != (rc = sqlite3_reset(pDeleteParent))) {
    return rc;
  }
  
  /* Remove the node from the in-memory hash table and link it into
  ** the pool of removed nodes. Its contents will be re-inserted later on.
  */
  node_hash_remove(node);
  node->nodeid = height;
  node->n_ref++; // why?
  removed_nodes.push(node);

  return SQLITE_OK;
}

int RDtreeVtab::fix_node_bounds(RDtreeNode *node)
{
  int rc = SQLITE_OK; 
  RDtreeNode *parent = node->parent;
  if (parent) {
    int ii; 
    int nItem = node->get_size();
    RDtreeItem bounds(bfp_bytes);  /* Bounding box for node */
    node->get_item(0, &bounds);
    for (ii = 1; ii < nItem; ii++) {
      RDtreeItem item(bfp_bytes);
      node->get_item(ii, &item);
      bounds.extend_bounds(item);
    }
    bounds.rowid = node->nodeid;
    rc = node->get_index_in_parent(&ii);
    if (rc == SQLITE_OK) {
      parent->overwrite_item(ii, &bounds);
      rc = fix_node_bounds(parent);
    }
  }
  return rc;
}

/*
** Delete the item at index iItem of node pNode. After removing the
** item, adjust the rd-tree data structure if required.
*/
int RDtreeVtab::delete_item(RDtreeNode *node, int idx, int height)
{
  int rc;

  /*
  ** If node is not the root and its parent is null, load all the ancestor
  ** nodes into memory
  */
  if ((rc = fix_leaf_parent(node)) != SQLITE_OK) {
    return rc;
  }

  /* Remove the item from the node. This call just moves bytes around
  ** the in-memory node image, so it cannot fail.
  */
  node->delete_item(idx);

  /* If the node is not the tree root and now has less than the minimum
  ** number of cells, remove it from the tree. Otherwise, update the
  ** cell in the parent node so that it tightly contains the updated
  ** node.
  */
  RDtreeNode *parent = node->parent;
  assert(parent || node->nodeid == 1);
  if (parent) {
    if (node->get_size() < node_minsize()) {
      rc = remove_node(node, height);
    }
    else {
      rc = fix_node_bounds(node);
    }
  }

  return rc;
}

/*
** Insert item pItem into node pNode. Node pNode is the head of a 
** subtree iHeight high (leaf nodes have iHeight==0).
*/
int RDtreeVtab::insert_item(RDtreeNode *node, RDtreeItem *item, int height)
{
  int rc = SQLITE_OK;

  if (height > 0) {
    RDtreeNode *child = node_hash_lookup(item->rowid);
    if (child) {
      node_decref(child->parent);
      node_incref(node);
      child->parent = node;
    }
  }

  if (node->insert_item(item)) {
    /* node was full */
    rc = split_node(node, item, height);
  }
  else {
    /* insertion succeded */
    rc = adjust_tree(node, item);
    if (rc == SQLITE_OK) {
      if (height == 0) {
        rc = rowid_write(item->rowid, node->nodeid);
      }
      else {
        rc = parent_write(item->rowid, node->nodeid);
      }
    }
  }

  return rc;
}

int RDtreeVtab::reinsert_node_content(RDtreeNode *node)
{
  int ii;
  int rc = SQLITE_OK;
  int nItem = node->get_size();

  for (ii = 0; rc == SQLITE_OK && ii < nItem; ii++) {
    RDtreeItem item(bfp_bytes);
    node->get_item(ii, &item);

    /* Find a node to store this cell in. node->nodeid currently contains
    ** the height of the sub-tree headed by the cell.
    */
    RDtreeNode *insert;
    rc = strategy->choose_leaf(&item, (int)node->nodeid, &insert);
    if (rc == SQLITE_OK) {
      int rc2;
      rc = insert_item(insert, &item, (int)node->nodeid);
      rc2 = node_decref(insert);
      if (rc==SQLITE_OK) {
        rc = rc2;
      }
    }
  }
  return rc;
}


/*
** Remove the entry with rowid=rowid from the rd-tree structure.
*/
int RDtreeVtab::delete_rowid(sqlite3_int64 rowid) 
{
  int rc, rc2;                    /* Return code */
  RDtreeNode *leaf = 0;          /* Leaf node containing record rowid */
  int item;                      /* Index of rowid item in leaf */
  RDtreeNode *root;              /* Root node of rtree structure */


  /* Obtain a reference to the root node to initialise RDtree.iDepth */
  rc = node_acquire(1, 0, &root);

  /* Obtain a reference to the leaf node that contains the entry 
  ** about to be deleted. 
  */
  if (rc == SQLITE_OK) {
    rc = find_leaf_node(rowid, &leaf);
  }

  /* Delete the cell in question from the leaf node. */
  if (rc == SQLITE_OK) {
    rc = leaf->get_rowid_index(rowid, &item);
    if (rc == SQLITE_OK) {
      const uint8_t *bfp = leaf->get_bfp(item);
      rc = decrement_bitfreq(bfp);
    }
    if (rc == SQLITE_OK) {
      int weight = leaf->get_max_weight(item);
      rc = decrement_weightfreq(weight);
    }
    if (rc == SQLITE_OK) {
      rc = delete_item(leaf, item, 0);
    }
    rc2 = node_decref(leaf);
    if (rc == SQLITE_OK) {
      rc = rc2;
    }
  }

  /* Delete the corresponding entry in the <rdtree>_rowid table. */
  if (rc == SQLITE_OK) {
    sqlite3_bind_int64(pDeleteRowid, 1, rowid);
    sqlite3_step(pDeleteRowid);
    rc = sqlite3_reset(pDeleteRowid);
  }

  /* Check if the root node now has exactly one child. If so, remove
  ** it, schedule the contents of the child for reinsertion and 
  ** reduce the tree height by one.
  **
  ** This is equivalent to copying the contents of the child into
  ** the root node (the operation that Gutman's paper says to perform 
  ** in this scenario).
  */
  if (rc == SQLITE_OK && depth > 0 && root->get_size() == 1) {
    RDtreeNode *child;
    sqlite3_int64 child_rowid = root->get_rowid(0);
    rc = node_acquire(child_rowid, root, &child);
    if( rc==SQLITE_OK ){
      rc = remove_node(child, depth - 1);
    }
    rc2 = node_decref(child);
    if (rc == SQLITE_OK) {
      rc = rc2;
    }
    if (rc == SQLITE_OK) {
      --depth;
      write_uint16(root->data.data(), depth);
      root->dirty = true;
    }
  }

  /* Re-insert the contents of any underfull nodes removed from the tree. */
  while (!removed_nodes.empty()) {
    RDtreeNode *removed_node = removed_nodes.top();
    if (rc == SQLITE_OK) {
      // CHECK: can this operation remove other nodes
      // during this same loop?
      rc = reinsert_node_content(removed_node);
    }
    delete removed_node;
    removed_nodes.pop();
  }

  /* Release the reference to the root node. */
  rc2 = node_decref(root);
  if (rc == SQLITE_OK) {
    rc = rc2;
  }

  return rc;
}

/*
** The xUpdate method for rdtree module virtual tables.
*/
int RDtreeVtab::update(int argc, sqlite3_value **argv, sqlite_int64 *updated_rowid)
{
  int rc = SQLITE_OK;
  RDtreeItem item(bfp_bytes);                /* New item to insert if argc>1 */
  bool have_rowid = false;        /* Set to true after new rowid is determined */

  incref();

  /*
  ** The number of args can be either 1, for a pure delete operation, or 2+N - where N
  ** is the number of columns in the table - for an insert, update or replace operation.
  **
  ** In this case it's then either 1 or 4.
  */
  assert(argc == 1 || argc == 4);

  /*
  ** argc = 1
  ** argv[0] != NULL
  ** DELETE the row with rowid equal argv[0], no insert
  **
  ** argc > 1
  ** argv[0] == NULL
  ** INSERT A new row is inserted with column values taken from argv[2] and following.
  ** In a rowid virtual table, if argv[1] is an SQL NULL, then a new unique rowid is
  ** generated automatically.
  **
  ** argc > 1
  ** argv[0] != NULL
  ** argv[0] == argv[1]
  ** UPDATE The row with rowid argv[0] is updated with new values in argv[2] and
  ** following parameters.
  **
  ** argc > 1
  ** argv[0] != NULL
  ** argv[0] != argv[1]
  ** UPDATE with rowid change: The row with rowid argv[0] is updated with the rowid in
  ** argv[1] and new values in argv[2] and following parameters.
  */

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
    
    sqlite3_int64 rowid = 0;

    /* If a rowid value was supplied, check if it is already present in 
    ** the table. If so, the constraint has failed. */
    if (sqlite3_value_type(argv[2]) != SQLITE_NULL ) {

      rowid = sqlite3_value_int64(argv[2]);

      if ((sqlite3_value_type(argv[0]) == SQLITE_NULL) || (sqlite3_value_int64(argv[0]) != rowid)) {
        sqlite3_bind_int64(pReadRowid, 1, rowid);
        int steprc = sqlite3_step(pReadRowid);
        rc = sqlite3_reset(pReadRowid);
        if (SQLITE_ROW == steprc) { /* rowid already exists */
          if (sqlite3_vtab_on_conflict(db) == SQLITE_REPLACE) {
            rc = delete_rowid(rowid);
          }
	        else {
            rc = SQLITE_CONSTRAINT;
            goto update_end;
          }
        }
      }

      have_rowid = true;
    }

    std::string bfp = arg_to_bfp(argv[3], &rc);
    int input_bfp_bytes = bfp.size();
    if (rc == SQLITE_OK && input_bfp_bytes != bfp_bytes) {
      rc = SQLITE_MISMATCH;
    }
    else {
      if (have_rowid) {
        item.rowid = rowid;
      }
      // FIXME
      item.bfp.resize(bfp_bytes);
      memcpy(item.bfp.data(), bfp.data(), bfp_bytes);
      item.min_weight = item.max_weight = bfp_op_weight(bfp_bytes, item.bfp.data());
    }

    if (rc != SQLITE_OK) {
      goto update_end;
    }
  }  // if (argc > 1)

  /* If argv[0] is not an SQL NULL value, it is the rowid of a
  ** record to delete from the r-tree table. The following block does
  ** just that.
  */
  if (sqlite3_value_type(argv[0]) != SQLITE_NULL) {
    rc = delete_rowid(sqlite3_value_int64(argv[0]));
  }

  /* If the argv[] array contains more than one element, elements
  ** (argv[2]..argv[argc-1]) contain a new record to insert into
  ** the rd-tree structure.
  */
  if ((rc == SQLITE_OK) && (argc > 1)) {
    /* Insert the new record into the rd-tree */
    RDtreeNode *pLeaf = 0;

    /* Figure out the rowid of the new row. */
    if (!have_rowid) {
      rc = new_rowid(&item.rowid);
    }
    *updated_rowid = item.rowid;

    if (rc == SQLITE_OK) {
      rc = strategy->choose_leaf(&item, 0, &pLeaf);
    }

    if (rc == SQLITE_OK) {
      int rc2;
      rc = insert_item(pLeaf, &item, 0);
      rc2 = node_decref(pLeaf);
      if (rc == SQLITE_OK) {
        rc = rc2;
      }
    }

    if (rc == SQLITE_OK) {
      rc = increment_bitfreq(item.bfp.data());
    }
    if (rc == SQLITE_OK) {
      rc = increment_weightfreq(item.max_weight);
    }
  }

update_end:
  decref();
  return rc;
}

/* 
** rdtree virtual table module xOpen method.
*/
int RDtreeVtab::open(sqlite3_vtab_cursor **cursor)
{
  int rc = SQLITE_OK;
  RDtreeCursor *csr = new RDtreeCursor; // FIXME: try/catch set rc = SQLITE_NOMEM
  csr->pVtab = this;
  *cursor = csr;
  return rc;
}

/*
** Free the RDtreeCursor.aConstraint[] array and its contents.
*/
/*
maybe used by RDtreeVtab::close() see below

static void freeCursorConstraints(RDtreeCursor *csr)
{
  if (csr->aConstraint) {
    sqlite3_free(csr->aConstraint);
    csr->aConstraint = 0;
  }
}*/

/* 
** rdtree virtual table module xClose method.
*/
int RDtreeVtab::close(sqlite3_vtab_cursor *cursor)
{
  RDtreeCursor *csr = (RDtreeCursor *)cursor;
  // maybe later freeCursorConstraints(csr);
  int rc = node_decref(csr->node);
  delete csr;
  return rc;
}

int RDtreeVtab::test_item(RDtreeCursor *csr, int height, bool *is_eof)
{
  RDtreeItem item(bfp_bytes);
  int rc = SQLITE_OK;

  csr->node->get_item(csr->item, &item);

  *is_eof = false;
  for (auto p: csr->constraints) {
    if (height == 0) {
      rc = p->op->test_leaf(this, *p, &item, is_eof);
    }
    else {
      rc = p->op->test_internal(this, *p, &item, is_eof);
    }
    if (!is_eof) {
      break;
    }
  }
#if 0
  // replacing..
  for (int ii = 0; *is_eof == false && rc == SQLITE_OK && ii < csr->nConstraint; ii++) {
    RDtreeConstraint *p = &csr->aConstraint[ii];
    if (height == 0) {
      rc = p->op->test_leaf(this, p, &item, is_eof);
    }
    else {
      rc = p->op->test_internal(this, p, &item, is_eof);
    }
  }
#endif

  return rc;
}

/*
** Cursor pCursor currently points at a node that heads a sub-tree of
** height iHeight (if iHeight==0, then the node is a leaf). Descend
** to point to the left-most cell of the sub-tree that matches the 
** configured constraints.
*/
int RDtreeVtab::descend_to_item(RDtreeCursor *csr, int height, bool *is_eof)
{
  assert(height >= 0);

  RDtreeNode *saved_node = csr->node;
  int saved_item = csr->item;

  int rc = test_item(csr, height, is_eof);

  if (rc != SQLITE_OK || *is_eof || height==0) {
    return rc;
  }

  RDtreeNode *child;
  sqlite3_int64 rowid = csr->node->get_rowid(csr->item);
  rc = node_acquire(rowid, csr->node, &child);
  if (rc != SQLITE_OK) {
    return rc;
  }

  node_decref(csr->node);
  csr->node = child;
  *is_eof = true; // useless? defensive?
  int num_items = child->get_size();
  for (int ii=0; *is_eof && ii < num_items; ii++) {
    csr->item = ii;
    rc = descend_to_item(csr, height-1, is_eof);
    if (rc != SQLITE_OK) {
      return rc;
    }
  }

  if (*is_eof) {
    assert(csr->node == child);
    node_incref(saved_node);
    node_decref(child);
    csr->node = saved_node;
    csr->item = saved_item;
  }

  return rc;
}

/* 
** rdtree virtual table module xNext method.
*/
int RDtreeVtab::next(sqlite3_vtab_cursor *cursor)
{
  RDtreeCursor *csr = (RDtreeCursor *)cursor;
  int rc = SQLITE_OK;

  /* RDtreeCursor.node must not be NULL. If it is NULL, then this cursor is
  ** already at EOF. It is against the rules to call the xNext() method of
  ** a cursor that has already reached EOF.
  */
  assert(csr->node);

  if (csr->strategy == 1) {
    /* This "scan" is a direct lookup by rowid. There is no next entry. */
    node_decref(csr->node);
    csr->node = 0;
  }
  else {
    /* Move to the next entry that matches the configured constraints. */
    int height = 0;
    while (csr->node) {
      RDtreeNode *node = csr->node;
      int num_items = node->get_size();
      for (csr->item++; csr->item < num_items; csr->item++) {
        bool is_eof;
        rc = descend_to_item(csr, height, &is_eof);
        if (rc != SQLITE_OK || !is_eof) {
          return rc;
        }
      }
      csr->node = node->parent;
      rc = node->get_index_in_parent(&csr->item);
      if (rc != SQLITE_OK) {
        return rc;
      }
      node_incref(csr->node);
      node_decref(node);
      ++height;
    }
  }

  return rc;
}

/* 
** RDtree virtual table module xFilter method.
*/
int RDtreeVtab::filter(
      sqlite3_vtab_cursor *cursor, 
			int idxnum, const char */*idxstr*/,
			int argc, sqlite3_value **argv)
{
  RDtreeCursor *csr = (RDtreeCursor *)cursor;

  int rc = SQLITE_OK;

  incref();

  // needed? or not needed? freeCursorConstraints(csr);
  csr->strategy = idxnum;

  if (csr->strategy == 1) {
    /* Special case - lookup by rowid. */
    RDtreeNode *leaf;        /* Leaf on which the required item resides */
    sqlite3_int64 rowid = sqlite3_value_int64(argv[0]);
    rc = find_leaf_node(rowid, &leaf);
    csr->node = leaf; 
    if (leaf) {
      assert(rc == SQLITE_OK);
      rc = leaf->get_rowid_index(rowid, &csr->item);
    }
  }
  else {
    /* Normal case - rd-tree scan. Set up the RDtreeCursor.aConstraint array 
    ** with the configured constraints. 
    */
    if (argc > 0) {
      #if 0 // FIXME
      csr->aConstraint = sqlite3_malloc(sizeof(RDtreeConstraint) * argc);
      csr->nConstraint = argc;
      if (!csr->aConstraint) {
        rc = SQLITE_NOMEM;
      }
      else {
        memset(csr->aConstraint, 0, sizeof(RDtreeConstraint) * argc);
        for (int ii = 0; ii < argc; ii++) {
          RDtreeConstraint *p = &csr->aConstraint[ii];
          /* A MATCH operator. The right-hand-side must be a blob that
          ** can be cast into an RDtreeMatchArg object.
          */
          rc = deserializeMatchArg(argv[ii], p);
          if (rc == SQLITE_OK && p->op->xInitializeConstraint) {
              rc = p->op->xInitializeConstraint(pRDtree, p);
          }
          if (rc != SQLITE_OK) {
            break;
          }
        }
      }
      #endif
    }
  
    RDtreeNode *root = 0;

    if (rc == SQLITE_OK) {
      csr->node = 0;
      rc = node_acquire(1, 0, &root);
    }
    if (rc == SQLITE_OK) {
      bool is_eof = true;
      int num_items = root->get_size();
      csr->node = root;
      for (csr->item = 0; 
	      rc == SQLITE_OK && csr->item < num_items; csr->item++) {
        assert(csr->node == root);
        rc = descend_to_item(csr, depth, &is_eof);
        if (!is_eof) {
          break;
        }
      }
      if (rc == SQLITE_OK && is_eof) {
        assert( csr->node == root );
        node_decref(root);
        csr->node = 0;
      }
      assert(rc != SQLITE_OK || !csr->node || csr->item < csr->node->get_size());
    }
  }

  decref();
  return rc;
}

/*
** rdtree virtual table module xEof method.
**
** Return non-zero if the cursor does not currently point to a valid 
** record (i.e if the scan has finished), or zero otherwise.
*/
int RDtreeVtab::eof(sqlite3_vtab_cursor *cursor)
{
  RDtreeCursor *csr = (RDtreeCursor *)cursor;
  return (csr->node == 0);
}

/* 
** rdtree virtual table module xRowid method.
*/
int RDtreeVtab::rowid(sqlite3_vtab_cursor *vtab_cursor, sqlite_int64 *rowid_result)
{
  RDtreeCursor *csr = (RDtreeCursor *)vtab_cursor;

  assert(csr->node);
  *rowid_result = csr->node->get_rowid(csr->item);

  return SQLITE_OK;
}

/* 
** rdtree virtual table module xColumn method.
*/
int RDtreeVtab::column(sqlite3_vtab_cursor *vtab_cursor, sqlite3_context *ctx, int col)
{
  int rc = SQLITE_OK;
  RDtreeCursor *csr = (RDtreeCursor *)vtab_cursor;

  if (col == 0) {
    sqlite3_int64 rowid = csr->node->get_rowid(csr->item);
    sqlite3_result_int64(ctx, rowid);
  }
  else {
    const uint8_t *data = csr->node->get_bfp(csr->item);
    std::string bfp(data, data+bfp_bytes); // FIXME (not pretty)
    Blob blob = bfp_to_blob(bfp, &rc);
    if (rc == SQLITE_OK) {
      sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
    }
  }

  return SQLITE_OK;
}

/*
** rdtree virtual table module xRename method.
*/
int RDtreeVtab::rename(const char *newname)
{
  int rc = SQLITE_NOMEM;
  char *sql = sqlite3_mprintf(
    "ALTER TABLE %Q.'%q_node'   RENAME TO \"%w_node\";"
    "ALTER TABLE %Q.'%q_parent' RENAME TO \"%w_parent\";"
    "ALTER TABLE %Q.'%q_rowid'  RENAME TO \"%w_rowid\";"
    "ALTER TABLE %Q.'%q_bitfreq'  RENAME TO \"%w_bitfreq\";"
    "ALTER TABLE %Q.'%q_weightfreq'  RENAME TO \"%w_weightfreq\";"
    , db_name.c_str(), table_name.c_str(), newname 
    , db_name.c_str(), table_name.c_str(), newname 
    , db_name.c_str(), table_name.c_str(), newname 
    , db_name.c_str(), table_name.c_str(), newname 
    , db_name.c_str(), table_name.c_str(), newname 
  );
  if (sql) {
    rc = sqlite3_exec(db, sql, 0, 0, 0);
    sqlite3_free(sql);
  }
  return rc;
}

/*
** Select a currently unused rowid for a new rd-tree record.
*/
int RDtreeVtab::new_rowid(sqlite3_int64 *rowid)
{
  int rc;
  sqlite3_bind_null(pWriteRowid, 1);
  sqlite3_bind_null(pWriteRowid, 2);
  sqlite3_step(pWriteRowid);
  rc = sqlite3_reset(pWriteRowid);
  *rowid = sqlite3_last_insert_rowid(db);
  return rc;
}

void RDtreeVtab::incref()
{
  ++n_ref;
}

void RDtreeVtab::decref()
{
  --n_ref;
  if (n_ref == 0) {
    sqlite3_finalize(pReadNode);
    sqlite3_finalize(pWriteNode);
    sqlite3_finalize(pDeleteNode);
    sqlite3_finalize(pReadRowid);
    sqlite3_finalize(pWriteRowid);
    sqlite3_finalize(pDeleteRowid);
    sqlite3_finalize(pReadParent);
    sqlite3_finalize(pWriteParent);
    sqlite3_finalize(pDeleteParent);
    sqlite3_finalize(pIncrementBitfreq);
    sqlite3_finalize(pDecrementBitfreq);
    sqlite3_finalize(pIncrementWeightfreq);
    sqlite3_finalize(pDecrementWeightfreq);
    delete this; /* !!! */
  }
}
