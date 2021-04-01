#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

#include "rdtree_vtab.hpp"
#include "rdtree_node.hpp"
#include "rdtree_item.hpp"
#include "bfp_ops.hpp"

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
static const unsigned int RDTREE_OPT_FOR_SUBSET_QUERIES = 1;
static const unsigned int RDTREE_OPT_FOR_SIMILARITY_QUERIES = 2;

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
  int rc = SQLITE_OK;
  RDtreeVtab *rdtree;

  int bfp_size; /* Length (in bytes) of stored binary fingerprint */

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

  int sz;
  if (sscanf(argv[4], "%*s bits( %d )", &sz) == 1) {
      if (sz <= 0 || sz % 8) {
        *err = sqlite3_mprintf("invalid number of bits for a stored fingerprint: '%d'", sz);
        return SQLITE_ERROR;
      }
      bfp_size = sz/8;
  }
  else if (sscanf(argv[4], "%*s bytes( %d )", &sz) == 1) {
      if (sz <= 0) {
        *err = sqlite3_mprintf("invalid number of bytes for a stored fingerprint: '%d'", sz);
        return SQLITE_ERROR;
      }
      bfp_size = sz;
  }
  else {
    *err = sqlite3_mprintf("unable to parse the fingerprint size from: '%s'", argv[4]);
    return SQLITE_ERROR;
  }

  if (bfp_size > RDTREE_MAX_BITSTRING_SIZE) {
    *err = sqlite3_mprintf("the requested fingerpring size exceeds the supported max value: %d bytes", RDTREE_MAX_BITSTRING_SIZE);
    return SQLITE_ERROR;
  }

  unsigned int flags = RDTREE_FLAGS_UNASSIGNED;
  if (argc == 6) {
    if (strcmp(argv[5], "OPT_FOR_SUBSET_QUERIES") == 0) {
      flags |= RDTREE_OPT_FOR_SUBSET_QUERIES;
    }
    else if (strcmp(argv[5], "OPT_FOR_SIMILARITY_QUERIES") == 0) {
      flags |= RDTREE_OPT_FOR_SIMILARITY_QUERIES;
    }
    else {
      *err = sqlite3_mprintf("unrecognized option: %s", argv[5]);
      return SQLITE_ERROR;
    }
  }

  sqlite3_vtab_config(db, SQLITE_VTAB_CONSTRAINT_SUPPORT, 1);

  /* Allocate the sqlite3_vtab structure */
  rdtree = new RDtreeVtab; // FIXME try/catch?
  rdtree->db_name = argv[1];
  rdtree->table_name = argv[2];
  rdtree->db = db;
  rdtree->flags = flags;
  rdtree->bfp_size = bfp_size;
  rdtree->bytes_per_item = 8 /* row id */ + 4 /* min/max weight */ + bfp_size; 
  rdtree->n_ref = 1;

  /* Figure out the node size to use. */
  rc = rdtree->get_node_size(is_create);

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
** to. If successful, pRDtree->node_size is populated and SQLITE_OK returned.
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
int RDtreeVtab::get_node_size(int is_create)
{
  int rc;
  char *sql;
  if (is_create) {
    int page_size = 0;
    sql = sqlite3_mprintf("PRAGMA %Q.page_size", db_name.c_str());
    rc = select_int(db, sql, &page_size);
    if (rc==SQLITE_OK) {
      node_size = page_size - 64;
      if ((4 + bytes_per_item*RDTREE_MAXITEMS) < node_size) {
        node_size = 4 + bytes_per_item*RDTREE_MAXITEMS;
      }
    }
  }
  else{
    sql = sqlite3_mprintf("SELECT length(data) FROM '%q'.'%q_node' "
			   "WHERE nodeno=1", db_name.c_str(), table_name.c_str());
    rc = select_int(db, sql, &node_size);
  }

  node_capacity = (node_size - 4)/bytes_per_item;

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
			db_name.c_str(), table_name.c_str(), node_size
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

    for (int i=0; i < bfp_size*8; ++i) {
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

    for (int i=0; i <= bfp_size*8; ++i) {
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
int RDtreeVtab::increment_bitfreq(uint8_t *bfp)
{
  int rc = SQLITE_OK;
  
  int i, bitno = 0;
  uint8_t * bfp_end = bfp + bfp_size;
  
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
int RDtreeVtab::decrement_bitfreq(uint8_t *bfp)
{
  int rc = SQLITE_OK;
  
  int i, bitno = 0;
  uint8_t * bfp_end = bfp + bfp_size;
  
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
  RDtreeNode *node = new RDtreeNode; // FIXME
  node->data.resize(bfp_size);
  node->n_ref = 1;
  node->parent = parent;
  node->is_dirty = 1;
  node->next = nullptr;
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
  int rc2 = SQLITE_OK;

  /* Check if the requested node is already in the hash table. If so,
  ** increase its reference count and return it.
  */
  RDtreeNode * node = node_hash_lookup(nodeid);
  if (node) {
    assert( !parent || !node->parent || node->parent == parent );
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
    if (node_size == sqlite3_column_bytes(pReadNode, 0)) {
      node = new RDtreeNode; // FIXME
      node->parent = parent;
      node->n_ref = 1;
      node->nodeid = nodeid;
      node->is_dirty = 0;
      node->next = nullptr;
      node->data.resize(node_size);
      memcpy(node->data.data(), blob, node_size);
      node_incref(parent);
    }
  }

  rc = sqlite3_reset(pReadNode);
  if (rc == SQLITE_OK) {
    rc = rc2;
  }

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
    if (node->size() > node_capacity) {
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
  if (node->is_dirty) {
    if (node->nodeid) {
      sqlite3_bind_int64(pWriteNode, 1, node->nodeid);
    }
    else {
      sqlite3_bind_null(pWriteNode, 1);
    }
    sqlite3_bind_blob(pWriteNode, 2, node->data.data(), node_size, SQLITE_STATIC);
    sqlite3_step(pWriteNode);
    node->is_dirty = 0;
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
  assert(node);
  ++node->n_ref;
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
** Return the 64-bit integer value associated with item item of
** node pNode. If pNode is a leaf node, this is a rowid. If it is
** an internal node, then the 64-bit integer is a child page number.
*/
sqlite3_int64 RDtreeVtab::node_get_rowid(RDtreeNode *node, int item)
{
  assert(item < node->size());
  return read_uint64(&node->data.data()[4 + bytes_per_item*item]);
}


/*
** One of the items in node node is guaranteed to have a 64-bit 
** integer value equal to rowid. Return the index of this item.
*/
int RDtreeVtab::node_rowid_index(RDtreeNode *node, sqlite3_int64 rowid, int *index)
{
  int node_size = node->size();
  for (int ii = 0; ii < node_size; ++ii) {
    if (node_get_rowid(node, ii) == rowid) {
      *index = ii;
      return SQLITE_OK;
    }
  }
  return SQLITE_CORRUPT_VTAB;
}

/*
** Return pointer to the binary fingerprint associated with the given item of
** the given node. If node is a leaf node, this is a virtual table element.
** If it is an internal node, then the binary fingerprint defines the 
** bounds of a child node
*/
uint8_t *RDtreeVtab::node_get_bfp(RDtreeNode *node, int item)
{
  assert(item < node->size());
  return &node->data.data()[4 + bytes_per_item*item + 8 /* rowid */ + 4 /* min/max weight */];
}

/* Return the min weight computed on the fingerprints associated to this
** item. If pNode is a leaf node then this is the actual population count
** for the item's fingerprint. On internal nodes the min weight contributes
** to defining the cell bounds
*/
int RDtreeVtab::node_get_min_weight(RDtreeNode *node, int item)
{
  assert(item < node->size());
  return read_uint16(&node->data.data()[4 + bytes_per_item*item + 8]);
}

/* Return the max weight computed on the fingerprints associated to this
** item. If pNode is a leaf node then this is the actual population count
** for the item's fingerprint. On internal nodes the max weight contributes
** to defining the cell bounds
*/
int RDtreeVtab::node_get_max_weight(RDtreeNode *node, int item)
{
  assert(item < node->size());
  return read_uint16(&node->data.data()[4 + bytes_per_item*item + 8 /* rowid */ + 2 /* min weight */]);
}

/*
** Deserialize item iItem of node pNode. Populate the structure pointed
** to by pItem with the results.
*/
void RDtreeVtab::node_get_item(RDtreeNode *node, int idx, RDtreeItem *item)
{
  item->rowid = node_get_rowid(node, idx);
  item->min_weight = read_uint16(&node->data.data()[4 + bytes_per_item*idx + 8 /* rowid */]);
  item->max_weight = read_uint16(&node->data.data()[4 + bytes_per_item*idx + 8 /* rowid */ + 2 /* min weight */]);
  uint8_t *bfp = node_get_bfp(node, idx);
  item->bfp.assign(bfp, bfp+bfp_size);
}

/*
**
*/
double RDtreeVtab::item_weight_distance(RDtreeItem *a, RDtreeItem *b)
{
  int d1 = abs(a->min_weight - b->min_weight);
  int d2 = abs(a->max_weight - b->max_weight);
  return (double) (d1 + d2);
  /* return (double) (d1 > d2) ? d1 : d2; */
}

int RDtreeVtab::item_weight(RDtreeItem *item)
{
  return bfp_op_weight(bfp_size, item->bfp.data());
}

/*
** Return true if item p2 is a subset of item p1. False otherwise.
*/
int RDtreeVtab::item_contains(RDtreeItem *a, RDtreeItem *b)
{
  return (
    a->min_weight <= b->min_weight &&
	  a->max_weight >= b->max_weight &&
	  bfp_op_contains(bfp_size, a->bfp.data(), b->bfp.data()) );
}

/*
** Return the amount item pBase would grow by if it were unioned with pAdded.
*/
int RDtreeVtab::item_growth(RDtreeItem *base, RDtreeItem *added)
{
  return bfp_op_growth(bfp_size, base->bfp.data(), added->bfp.data());
}

/*
** Extend the bounds of p1 to contain p2
*/
void RDtreeVtab::item_extend_bounds(RDtreeItem *base, RDtreeItem *added)
{
  bfp_op_union(bfp_size, base->bfp.data(), added->bfp.data());
  if (base->min_weight > added->min_weight) { base->min_weight = added->min_weight; }
  if (base->max_weight < added->max_weight) { base->max_weight = added->max_weight; }
}

/*
** This function implements the chooseLeaf algorithm from Gutman[84].
** ChooseSubTree in r*tree terminology.
*/
int RDtreeVtab::choose_leaf_subset(
			    RDtreeItem *item, /* Item to insert into rdtree */
			    int height, /* Height of sub-tree at item */
			    RDtreeNode **leaf /* OUT: Selected leaf page */
			    )
{
  int rc;
  RDtreeNode *node;
  rc = node_acquire(1, 0, &node);

  for (int ii = 0; rc == SQLITE_OK && ii < (depth - height); ii++) {
    sqlite3_int64 best = 0;

    int min_growth = 0;
    int min_weight = 0;

    int node_size = node->size();
    RDtreeNode *child;

    //RDtreeItem *aItem = 0;

    /* Select the child node which will be enlarged the least if item
    ** is inserted into it. Resolve ties by choosing the entry with
    ** the smallest weight.
    */
    for (int idx = 0; idx < node_size; idx++) {
      RDtreeItem curr_item;
      int growth;
      int weight;
      node_get_item(node, idx, &curr_item);
      growth = item_growth(&curr_item, item);
      weight = item_weight(&curr_item);

      if (idx == 0 || growth < min_growth || (growth == min_growth && weight < min_weight) ) {
        min_growth = growth;
        min_weight = weight;
        best = curr_item.rowid;
      }
    }

    //sqlite3_free(aItem);
    rc = node_acquire(best, node, &child);
    node_decref(node);
    node = child;
  }

  *leaf = node;
  return rc;
}

int RDtreeVtab::choose_leaf_similarity(
		RDtreeItem *item,
		int height,
		RDtreeNode **leaf
		)
{
  int rc;
  RDtreeNode *node;
  rc = node_acquire(1, 0, &node);

  for (int ii = 0; rc == SQLITE_OK && ii < (depth - height); ii++) {
    sqlite3_int64 best = 0;

    int min_growth = 0;
    double min_distance = 0.;
    
    int node_size = node->size();
    RDtreeNode *child;

    /* Select the child node which will be enlarged the least if pItem
    ** is inserted into it.
    */
    for (int idx = 0; idx < node_size; idx++) {
      RDtreeItem curr_item;
      double distance;
      int growth;
      node_get_item(node, idx, &curr_item);
      distance = item_weight_distance(&curr_item, item);
      growth = item_growth(&curr_item, item);

      if (idx == 0 || distance < min_distance || (distance == min_distance && growth < min_growth) ) {
        min_distance = distance;
        min_growth = growth;
        best = curr_item.rowid;
      }
    }

    rc = node_acquire(best, node, &child);
    node_decref(node);
    node = child;
  }

  *leaf = node;
  return rc;
}

int RDtreeVtab::choose_leaf_generic(
	RDtreeItem *item, /* Item to insert into rdtree */
	int height, /* Height of sub-tree at pItem */
	RDtreeNode **leaf /* OUT: Selected leaf page */
	)
{
  int rc;
  RDtreeNode *node;
  rc = node_acquire(1, 0, &node);

  for (int ii = 0; rc == SQLITE_OK && ii < (depth - height); ii++) {
    sqlite3_int64 best = 0;

    int min_growth = 0;
    double min_distance = 0.;
    int min_weight = 0;
    
    int node_size = node->size();
    RDtreeNode *child;

    /* Select the child node which will be enlarged the least if pItem
    ** is inserted into it.
    */
    for (int idx = 0; idx < node_size; idx++) {
      RDtreeItem curr_item;      
      node_get_item(node, idx, &curr_item);
      
      int growth = item_growth(&curr_item, item);
      double distance = item_weight_distance(&curr_item, item);
      int weight = item_weight(&curr_item);
      
      if (idx == 0 || growth < min_growth ||
	        (growth == min_growth && distance < min_distance) ||
	        (growth == min_growth && distance == min_distance && weight < min_weight)) {
        min_growth = growth;
        min_distance = distance;
        min_weight = weight;
        best = curr_item.rowid;
      }
    }

    rc = node_acquire(best, node, &child);
    node_decref(node);
    node = child;
  }

  *leaf = node;
  return rc;
}

int RDtreeVtab::choose_leaf(
		RDtreeItem *item, /* Item to insert into rdtree */
		int height, /* Height of sub-tree rooted at pItem */
		RDtreeNode **leaf /* OUT: Selected leaf page */
		)
{
  int rc = SQLITE_ERROR;
  if (flags & RDTREE_OPT_FOR_SUBSET_QUERIES) {
    rc = choose_leaf_subset(item, height, leaf);
  }
  else if (flags & RDTREE_OPT_FOR_SIMILARITY_QUERIES) {
    rc = choose_leaf_similarity(item, height, leaf);
  }
  else {
    rc = choose_leaf_generic(item, height, leaf);
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
    RDtreeItem item;
    int idx;

    if (node_parent_index(p, &idx)) {
      return SQLITE_CORRUPT_VTAB;
    }

    node_get_item(parent, idx, &item);
    if (!item_contains(&item, new_item)) {
      item_extend_bounds(&item, new_item);
      node_overwrite_item(parent, &item, idx);
    }
 
    p = parent;
  }
  return SQLITE_OK;
}

/*
** Overwrite item iItem of node pNode with the contents of pItem.
*/
void RDtreeVtab::node_overwrite_item(RDtreeNode *node, RDtreeItem *item, int idx)
{
  uint8_t *p = &node->data.data()[4 + bytes_per_item*idx];
  p += write_uint64(p, item->rowid);
  p += write_uint16(p, item->min_weight);
  p += write_uint16(p, item->max_weight);
  memcpy(p, item->bfp.data(), bfp_size);
  node->is_dirty = 1;
}

/*
** Remove the item with index iItem from node pNode.
*/
void RDtreeVtab::node_delete_item(RDtreeNode *node, int iItem)
{
  uint8_t *dst = &node->data.data()[4 + bytes_per_item*iItem];
  uint8_t *src = &dst[bytes_per_item];
  int bytes = (node->size() - iItem - 1) * bytes_per_item;
  memmove(dst, src, bytes);
  write_uint16(&node->data.data()[2], node->size()-1);
  node->is_dirty = 1;
}

/*
** Insert the contents of item pItem into node pNode. If the insert
** is successful, return SQLITE_OK.
**
** If there is not enough free space in pNode, return SQLITE_FULL.
*/
int RDtreeVtab::node_insert_item(RDtreeNode *node, RDtreeItem *item)
{
  int node_size = node->size();  /* Current number of items in pNode */

  assert(node_size <= node_capacity);

  if (node_size < node_capacity) {
    node_overwrite_item(node, item, node_size);
    write_uint16(&node->data.data()[2], node_size+1);
    node->is_dirty = 1;
  } 

  return (node_size == node_capacity) ? SQLITE_FULL : SQLITE_OK;
}

/*
** Clear the content of node p (set all bytes to 0x00).
*/
void RDtreeVtab::node_zero(RDtreeNode *p)
{
  assert(p);
  memset(&p->data.data()[2], 0, node_size-2);
  p->is_dirty = 1;
}

/*
** Pick the next item to be inserted into one of the two subsets. Select the
** one associated to a strongest "preference" for one of the two.
*/
void RDtreeVtab::pick_next_generic(
			    RDtreeItem *aItem, int nItem, int *aiUsed,
			    RDtreeItem *pLeftSeed, RDtreeItem *pRightSeed,
			    RDtreeItem */*pLeftBounds*/, RDtreeItem */*pRightBounds*/,
			    RDtreeItem **ppNext, int *pPreferRight)
{
  int iSelect = -1;
  int preferRight = 0;
  double dMaxPreference = -1.;
  int ii;

  for(ii = 0; ii < nItem; ii++){
    if( aiUsed[ii]==0 ){
      double left 
        = 1. - bfp_op_tanimoto(bfp_size, 
			       aItem[ii].bfp.data(), pLeftSeed->bfp.data());
      double right 
        = 1. - bfp_op_tanimoto(bfp_size, 
			       aItem[ii].bfp.data(), pRightSeed->bfp.data());
      double diff = left - right;
      double preference = 0.;
      if ((left + right) > 0.) {
        preference = fabs(diff)/(left + right);
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

void RDtreeVtab::pick_next_subset(
			   RDtreeItem *aItem, int nItem, int *aiUsed,
			   RDtreeItem *pLeftSeed, RDtreeItem *pRightSeed,
			   RDtreeItem */*pLeftBounds*/, RDtreeItem */*pRightBounds*/,
			   RDtreeItem **ppNext, int *pPreferRight)
{
  int iSelect = -1;
  int preferRight = 0;
  double dMaxPreference = -1.;
  int ii;

  for(ii = 0; ii < nItem; ii++){
    if( aiUsed[ii]==0 ){
      double left 
        = 1. - bfp_op_tanimoto(bfp_size, 
			       aItem[ii].bfp.data(), pLeftSeed->bfp.data());
      double right 
        = 1. - bfp_op_tanimoto(bfp_size, 
			       aItem[ii].bfp.data(), pRightSeed->bfp.data());
      double diff = left - right;
      double preference = 0.;
      if ((left + right) > 0.) {
        preference = fabs(diff)/(left + right);
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

void RDtreeVtab::pick_next_similarity(
			       RDtreeItem *aItem, int nItem, int *aiUsed,
			       RDtreeItem *pLeftSeed, RDtreeItem *pRightSeed,
			       RDtreeItem */*pLeftBounds*/,
			       RDtreeItem */*pRightBounds*/,
			       RDtreeItem **ppNext, int *pPreferRight)
{
  int iSelect = -1;
  int preferRight = 0;
  double dMaxPreference = -1.;
  int ii;

  for(ii = 0; ii < nItem; ii++){
    if( aiUsed[ii]==0 ){
      double left = item_weight_distance(&aItem[ii], pLeftSeed);
      double right = item_weight_distance(&aItem[ii], pRightSeed);
      double diff = left - right;
      double sum = left + right;
      double preference = fabs(diff);
      if (sum) {
        preference /= (sum);
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

void RDtreeVtab::pick_next(
		     RDtreeItem *aItem, int nItem, int *aiUsed,
		     RDtreeItem *pLeftSeed, RDtreeItem *pRightSeed,
		     RDtreeItem *pLeftBounds, RDtreeItem *pRightBounds,
		     RDtreeItem **ppNext, int *pPreferRight)
{
  if (flags & RDTREE_OPT_FOR_SUBSET_QUERIES) {
    pick_next_subset(aItem, nItem, aiUsed,
		   pLeftSeed, pRightSeed, pLeftBounds, pRightBounds,
		   ppNext, pPreferRight);
  }
  else if (flags & RDTREE_OPT_FOR_SIMILARITY_QUERIES) {
    pick_next_similarity(aItem, nItem, aiUsed,
		       pLeftSeed, pRightSeed, pLeftBounds, pRightBounds,
		       ppNext, pPreferRight);
  }
  else {
    pick_next_generic(aItem, nItem, aiUsed,
		    pLeftSeed, pRightSeed, pLeftBounds, pRightBounds,
		    ppNext, pPreferRight);
  }
}

/*
** Pick the two most dissimilar fingerprints.
*/
void RDtreeVtab::pick_seeds_generic(RDtreeItem *aItem, int nItem, int *piLeftSeed, int *piRightSeed)
{
  int ii;
  int jj;

  int iLeftSeed = 0;
  int iRightSeed = 1;
  double dMaxDistance = 0.;

  for (ii = 0; ii < nItem; ii++) {
    for (jj = ii + 1; jj < nItem; jj++) {
      double tanimoto 
        = bfp_op_tanimoto(bfp_size, aItem[ii].bfp.data(), aItem[jj].bfp.data());
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

void RDtreeVtab::pick_seeds_subset(RDtreeItem *aItem, int nItem, int *piLeftSeed, int *piRightSeed)
{
  int ii;
  int jj;

  int iLeftSeed = 0;
  int iRightSeed = 1;
  double dMaxDistance = 0.;

  for (ii = 0; ii < nItem; ii++) {
    for (jj = ii + 1; jj < nItem; jj++) {
      double tanimoto 
        = bfp_op_tanimoto(bfp_size, aItem[ii].bfp.data(), aItem[jj].bfp.data());
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

void RDtreeVtab::pick_seeds_similarity(RDtreeItem *aItem, int nItem, int *piLeftSeed, int *piRightSeed)
{
  int ii;
  int jj;

  int iLeftSeed = 0;
  int iRightSeed = 1;
  double dDistance;
  double dMaxDistance = 0.;

  for (ii = 0; ii < nItem; ii++) {
    for (jj = ii + 1; jj < nItem; jj++) {

      dDistance = item_weight_distance(&aItem[ii], &aItem[jj]);
      
      if (dDistance > dMaxDistance) {
        iLeftSeed = ii;
        iRightSeed = jj;
        dMaxDistance = dDistance;
      }
    }
  }

  *piLeftSeed = iLeftSeed;
  *piRightSeed = iRightSeed;
}

void RDtreeVtab::pick_seeds(RDtreeItem *aItem, int nItem, int *piLeftSeed, int *piRightSeed)
{
  if (flags & RDTREE_OPT_FOR_SUBSET_QUERIES) {
    pick_seeds_subset(aItem, nItem, piLeftSeed, piRightSeed);
  }
  else if (flags & RDTREE_OPT_FOR_SIMILARITY_QUERIES) {
    pick_seeds_similarity(aItem, nItem, piLeftSeed, piRightSeed);
  }
  else {
    pick_seeds_generic(aItem, nItem, piLeftSeed, piRightSeed);
  }
}

int RDtreeVtab::assign_items(RDtreeItem *aItem, int nItem,
		      RDtreeNode *pLeft, RDtreeNode *pRight,
		      RDtreeItem *pLeftBounds, RDtreeItem *pRightBounds)
{
  int iLeftSeed = 0;
  int iRightSeed = 1;
  int i;

  std::vector<int> aiUsed(nItem, 0);

  pick_seeds(aItem, nItem, &iLeftSeed, &iRightSeed);

  *pLeftBounds = aItem[iLeftSeed];
  *pRightBounds = aItem[iRightSeed];
  node_insert_item(pLeft, &aItem[iLeftSeed]);
  node_insert_item(pRight, &aItem[iRightSeed]);
  aiUsed[iLeftSeed] = 1;
  aiUsed[iRightSeed] = 1;

  for(i = nItem - 2; i > 0; i--) {
    int iPreferRight;
    RDtreeItem *pNext;
    pick_next(aItem, nItem, aiUsed.data(), 
	     &aItem[iLeftSeed], &aItem[iRightSeed], pLeftBounds, pRightBounds,
	     &pNext, &iPreferRight);

    if ((node_minsize() - pRight->size() == i) || (iPreferRight > 0 && (node_minsize() - pLeft->size() != i))) {
      node_insert_item(pRight, pNext);
      item_extend_bounds(pRightBounds, pNext);
    }
    else {
      node_insert_item(pLeft, pNext);
      item_extend_bounds(pLeftBounds, pNext);
    }
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

/*
** Return the index of the parent's item containing a pointer to node pNode.
** If pNode is the root node, return -1.
*/
int RDtreeVtab::node_parent_index(RDtreeNode *node, int *index)
{
  RDtreeNode *parent = node->parent;
  if (parent) {
    return node_rowid_index(parent, node->nodeid, index);
  }
  *index = -1;
  return SQLITE_OK;
}

int RDtreeVtab::split_node(RDtreeNode *node, RDtreeItem *item, int height)
{
  int new_item_is_right = 0;

  int rc = SQLITE_OK;
  int node_size = node->size();

  RDtreeNode *left = 0;
  RDtreeNode *right = 0;

  RDtreeItem leftbounds;
  RDtreeItem rightbounds;

  /* Allocate an array and populate it with a copy of pItem and 
  ** all items from node left. Then zero the original node.
  */
  std::vector<RDtreeItem> items(node_size + 1); // TODO: try/catch
  for (int i = 0; i < node_size; i++) {
    node_get_item(node, i, &items[i]);
  }
  node_zero(node);
  items[node_size] = *item;
  node_size += 1;

  if (node->nodeid == 1) { /* splitting the root node */
    right = node_new(node);
    left = node_new(node);
    depth++;
    node->is_dirty = 1;
    write_uint16(node->data.data(), depth);
  }
  else {
    left = node;
    node_incref(left);
    right = node_new(left->parent);
  }

  if (!left || !right) {
    rc = SQLITE_NOMEM;
    node_decref(right);
    node_decref(left);
    return rc;
  }

  memset(left->data.data(), 0, node_size);
  memset(right->data.data(), 0, node_size);

  rc = assign_items(items.data(), node_size, left, right, &leftbounds, &rightbounds);

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
    rc = node_parent_index(left, &parent_idx);
    if (rc == SQLITE_OK) {
      node_overwrite_item(parent, &leftbounds, parent_idx);
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

  int right_size = right->size();
  for (int i = 0; i < right_size; i++) {
    sqlite3_int64 rowid = node_get_rowid(right, i);
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
    int left_size = left->size();
    for (int i = 0; i < left_size; i++) {
      sqlite3_int64 rowid = node_get_rowid(left, i);
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
  rc = node_parent_index(node, &item);
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
  ** the RDtree.pDeleted list. Its contents will be re-inserted later on.
  */
  node_hash_remove(node);
  node->nodeid = height;
  node->next = pDeleted;
  node->n_ref++;
  pDeleted = node;

  return SQLITE_OK;
}

int RDtreeVtab::fix_node_bounds(RDtreeNode *node)
{
  int rc = SQLITE_OK; 
  RDtreeNode *parent = node->parent;
  if (parent) {
    int ii; 
    int nItem = node->size();
    RDtreeItem bounds;  /* Bounding box for node */
    node_get_item(node, 0, &bounds);
    for (ii = 1; ii < nItem; ii++) {
      RDtreeItem item;
      node_get_item(node, ii, &item);
      item_extend_bounds(&bounds, &item);
    }
    bounds.rowid = node->nodeid;
    rc = node_parent_index(node, &ii);
    if (rc == SQLITE_OK) {
      node_overwrite_item(parent, &bounds, ii);
      rc = fix_node_bounds(parent);
    }
  }
  return rc;
}

/*
** Delete the item at index iItem of node pNode. After removing the
** item, adjust the rd-tree data structure if required.
*/
int RDtreeVtab::delete_item(RDtreeNode *node, int iItem, int height)
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
  node_delete_item(node, iItem);

  /* If the node is not the tree root and now has less than the minimum
  ** number of cells, remove it from the tree. Otherwise, update the
  ** cell in the parent node so that it tightly contains the updated
  ** node.
  */
  RDtreeNode *parent = node->parent;
  assert(parent || node->nodeid == 1);
  if (parent) {
    if (node->size() < node_minsize()) {
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

  if (node_insert_item(node, item)) {
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
  int nItem = node->size();

  for (ii = 0; rc == SQLITE_OK && ii < nItem; ii++) {
    RDtreeItem item;
    node_get_item(node, ii, &item);

    /* Find a node to store this cell in. node->nodeid currently contains
    ** the height of the sub-tree headed by the cell.
    */
    RDtreeNode *insert;
    rc = choose_leaf(&item, (int)node->nodeid, &insert);
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
    rc = node_rowid_index(leaf, rowid, &item);
    if (rc == SQLITE_OK) {
      uint8_t *bfp = node_get_bfp(leaf, item);
      rc = decrement_bitfreq(bfp);
    }
    if (rc == SQLITE_OK) {
      int weight = node_get_max_weight(leaf, item);
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
  if (rc == SQLITE_OK && depth > 0 && root->size() == 1) {
    RDtreeNode *child;
    sqlite3_int64 child_rowid = node_get_rowid(root, 0);
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
      root->is_dirty = 1;
    }
  }

  /* Re-insert the contents of any underfull nodes removed from the tree. */
  for (leaf = pDeleted; leaf; leaf = pDeleted) {
    if (rc == SQLITE_OK) {
      rc = reinsert_node_content(leaf);
    }
    pDeleted = leaf->next;
    delete leaf;
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
int RDtreeVtab::update(int argc, sqlite3_value **argv, sqlite_int64 *pRowid)
{
  int rc = SQLITE_OK;
  RDtreeItem item;                /* New item to insert if argc>1 */
  int bHaveRowid = 0;             /* Set to 1 after new rowid is determined */

  incref();
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

      bHaveRowid = 1;
    }

    if (sqlite3_value_type(argv[3]) != SQLITE_BLOB) {
      rc = SQLITE_MISMATCH;
    }
    else if (sqlite3_value_bytes(argv[3]) != bfp_size) {
      rc = SQLITE_MISMATCH;
    }
    else {
      if (bHaveRowid) {
        item.rowid = rowid;
      }
      memcpy(item.bfp.data(), sqlite3_value_blob(argv[3]), bfp_size);
      item.min_weight = item.max_weight = bfp_op_weight(bfp_size, item.bfp.data());
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
    if (bHaveRowid == 0) {
      rc = new_rowid(&item.rowid);
    }
    *pRowid = item.rowid;

    if (rc == SQLITE_OK) {
      rc = choose_leaf(&item, 0, &pLeaf);
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
