#include <cassert>
#include <cstdio>
#include <cstring>

#include "rdtree_vtab.hpp"
#include "rdtree_node.hpp"
#include "rdtree_item.hpp"

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
** Clear the content of node p (set all bytes to 0x00).
*/
void RDtreeVtab::node_zero(RDtreeNode *node)
{
  memset(&node->data.data()[2], 0, node_size-2);
  node->is_dirty = 1;
}

/*
** Search the node hash table for node iNode. If found, return a pointer
** to it. Otherwise, return 0.
*/
RDtreeNode * RDtreeVtab::node_hash_lookup(sqlite3_int64 node_id)
{
  RDtreeNode *p = nullptr;
  auto nit = node_hash.find(node_id);
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
  assert(node_hash.find(node->node_id) == node_hash.end());
  node_hash[node->node_id] = node;
}

/*
** Remove node pNode from the node hash table.
*/
void RDtreeVtab::node_hash_remove(RDtreeNode *node)
{
  if (node->node_id != 0) {
    node_hash.erase(node->node_id);
  }
}

/*
** Obtain a reference to an rd-tree node.
*/
int RDtreeVtab::node_acquire(
    sqlite3_int64 node_id, RDtreeNode *parent, RDtreeNode **acquired)
{
  int rc;
  int rc2 = SQLITE_OK;

  /* Check if the requested node is already in the hash table. If so,
  ** increase its reference count and return it.
  */
  RDtreeNode * node = node_hash_lookup(node_id);
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

  sqlite3_bind_int64(pReadNode, 1, node_id);
  rc = sqlite3_step(pReadNode);

  if (rc == SQLITE_ROW) {
    const uint8_t *blob = (const uint8_t *)sqlite3_column_blob(pReadNode, 0);
    if (node_size == sqlite3_column_bytes(pReadNode, 0)) {
      node = new RDtreeNode; // FIXME
      node->parent = parent;
      node->n_ref = 1;
      node->node_id = node_id;
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
  if (node && node_id == 1) {
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
    if (node->node_id) {
      sqlite3_bind_int64(pWriteNode, 1, node->node_id);
    }
    else {
      sqlite3_bind_null(pWriteNode, 1);
    }
    sqlite3_bind_blob(pWriteNode, 2, node->data.data(), node_size, SQLITE_STATIC);
    sqlite3_step(pWriteNode);
    node->is_dirty = 0;
    rc = sqlite3_reset(pWriteNode);
    if (node->node_id == 0 && rc == SQLITE_OK) {
      node->node_id = sqlite3_last_insert_rowid(db);
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
  if (node->node_id == 1) {
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

#if 0
/*
** Remove the entry with rowid=rowid from the rd-tree structure.
*/
int RDtreeVtab::delete_rowid(sqlite3_int64 rowid) 
{
  int rc, rc2;                    /* Return code */
  RDtreeNode *pLeaf = 0;          /* Leaf node containing record rowid */
  int iItem;                      /* Index of rowid item in pLeaf */
  RDtreeNode *pRoot;              /* Root node of rtree structure */


  /* Obtain a reference to the root node to initialise RDtree.iDepth */
  rc = node_acquire(1, 0, &pRoot);

  /* Obtain a reference to the leaf node that contains the entry 
  ** about to be deleted. 
  */
  if (rc == SQLITE_OK) {
    rc = findLeafNode(pRDtree, rowid, &pLeaf);
  }

  /* Delete the cell in question from the leaf node. */
  if (rc == SQLITE_OK) {
    rc = nodeRowidIndex(pRDtree, pLeaf, rowid, &iItem);
    if (rc == SQLITE_OK) {
      u8 *pBfp = nodeGetBfp(pRDtree, pLeaf, iItem);
      rc = decrement_bitfreq(pBfp);
    }
    if (rc == SQLITE_OK) {
      int weight = nodeGetMaxWeight(pRDtree, pLeaf, iItem);
      rc = decrement_weightfreq(weight);
    }
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
    sqlite3_bind_int64(pRDtree->pDeleteRowid, 1, rowid);
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

      if ((sqlite3_value_type(argv[0]) == SQLITE_NULL) ||
	  (sqlite3_value_int64(argv[0]) != rowid)) {
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

    if (rc == SQLITE_OK) {
      rc = increment_bitfreq(pRDtree, item.aBfp);
    }
    if (rc == SQLITE_OK) {
      rc = increment_weightfreq(pRDtree, item.iMaxWeight);
    }
  }

update_end:
  decref();
  return rc;
}
#endif

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
