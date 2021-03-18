#include <cstdio>
#include <cstring>

#include "rdtree_vtab.hpp"

static const int RDTREE_MAX_BITSTRING_SIZE = 256;
static const int RDTREE_MAXITEMS = 51;

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