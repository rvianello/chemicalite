#include <cstdio>

#include "rdtree_vtab.hpp"

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
  sqlite3_vtab **ppVtab, char **pzErr, int isCreate)
{
  int rc = SQLITE_OK;
  RDtreeVtab *pRDtree;
  //int nDb;              /* Length of string argv[1] */
  //int nName;            /* Length of string argv[2] */

  //int iBfpSize;         /* Length (in bytes) of stored binary fingerprint */

  /* perform arg checking */
  if (argc < 5) {
    *pzErr = sqlite3_mprintf("wrong number of arguments. "
                             "two column definitions are required.");
    return SQLITE_ERROR;
  }
  if (argc > 6) {
    *pzErr = sqlite3_mprintf("wrong number of arguments. "
                             "at most one optional argument is expected.");
    return SQLITE_ERROR;
  }

  int sz;
  if (sscanf(argv[4], "%*s bits( %d )", &sz) == 1) {
      if (sz <= 0 || sz % 8) {
        *pzErr = sqlite3_mprintf("invalid number of bits for a stored fingerprint: '%d'", sz);
        return SQLITE_ERROR;
      }
      //iBfpSize = sz/8;
  }
  else if (sscanf(argv[4], "%*s bytes( %d )", &sz) == 1) {
      if (sz <= 0) {
        *pzErr = sqlite3_mprintf("invalid number of bytes for a stored fingerprint: '%d'", sz);
        return SQLITE_ERROR;
      }
      //iBfpSize = sz;
  }
  else {
    *pzErr = sqlite3_mprintf("unable to parse the fingerprint size from: '%s'", argv[4]);
    return SQLITE_ERROR;
  }

#if 0 // TODO
  if (iBfpSize > RDTREE_MAX_BITSTRING_SIZE) {
    *pzErr = sqlite3_mprintf("the requested fingerpring size exceeds the supported max value: %d bytes", RDTREE_MAX_BITSTRING_SIZE);
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
      *pzErr = sqlite3_mprintf("unrecognized option: %s", argv[5]);
      return SQLITE_ERROR;
    }
  }
#endif

  sqlite3_vtab_config(db, SQLITE_VTAB_CONSTRAINT_SUPPORT, 1);

  /* Allocate the sqlite3_vtab structure */
  //nDb = (int)strlen(argv[1]);
  //nName = (int)strlen(argv[2]);
  pRDtree = new RDtreeVtab;
  /*pRDtree = (RDtree *)sqlite3_malloc(sizeof(RDtree)+nDb+nName+2);
  if (!pRDtree) {
    return SQLITE_NOMEM;
  }
  memset(pRDtree, 0, sizeof(RDtree)+nDb+nName+2);*/

#if 0
  pRDtree->db = db;
  pRDtree->flags = flags;
  pRDtree->iBfpSize = iBfpSize;
  pRDtree->nBytesPerItem = 8 /* row id */ + 4 /* min/max weight */ + iBfpSize; 
  pRDtree->nBusy = 1;
  pRDtree->zDb = (char *)&pRDtree[1];
  pRDtree->zName = &pRDtree->zDb[nDb+1];
  memcpy(pRDtree->zDb, argv[1], nDb);
  memcpy(pRDtree->zName, argv[2], nName);

  /* Figure out the node size to use. */
  rc = getNodeSize(pRDtree, isCreate);
#endif

  /* Create/Connect to the underlying relational database schema. If
  ** that is successful, call sqlite3_declare_vtab() to configure
  ** the rd-tree table schema.
  */
  if (rc == SQLITE_OK) {
    if ((rc = pRDtree->sql_init(isCreate))) {
      *pzErr = sqlite3_mprintf("%s", sqlite3_errmsg(db));
    } 
    else {
      char *zSql = sqlite3_mprintf("CREATE TABLE x(%s", argv[3]);
      char *zTmp;
      /* the current implementation requires 2 columns specs, plus
      ** an optional flag. in practice, the following loop will always
      ** execute one single iteration, but I'm leaving it here although
      ** more generic than needed, just in case it may be useful again at
      ** some point in the future
      */
      int ii;
      /* for(ii=4; zSql && ii<argc; ii++){ */
      for(ii=4; zSql && ii<5; ii++){
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

  if (rc==SQLITE_OK) {
    *ppVtab = (sqlite3_vtab *)pRDtree;
  }
  else {
    pRDtree->decref();
  }

  return rc;
}

int RDtreeVtab::sql_init(int /*isCreate*/)
{
  int rc = SQLITE_OK;
#if 0
  #define N_STATEMENT 13
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
    "DELETE FROM '%q'.'%q_parent' WHERE nodeno = :1",

    /* Update the xxx_bitfreq table */
    "UPDATE '%q'.'%q_bitfreq' SET freq = freq + 1 WHERE bitno = :1",
    "UPDATE '%q'.'%q_bitfreq' SET freq = freq - 1 WHERE bitno = :1",

    /* Update the xxx_weightfreq table */
    "UPDATE '%q'.'%q_weightfreq' SET freq = freq + 1 WHERE weight = :1",
    "UPDATE '%q'.'%q_weightfreq' SET freq = freq - 1 WHERE weight = :1"
  };
  sqlite3_stmt **appStmt[N_STATEMENT];
  int i;

  if (isCreate) {
    char *zCreate 
      = sqlite3_mprintf("CREATE TABLE \"%w\".\"%w_node\"(nodeno INTEGER PRIMARY KEY, data BLOB);"
			"CREATE TABLE \"%w\".\"%w_rowid\"(rowid INTEGER PRIMARY KEY, nodeno INTEGER);"
			"CREATE TABLE \"%w\".\"%w_parent\"(nodeno INTEGER PRIMARY KEY, parentnode INTEGER);"
			"CREATE TABLE \"%w\".\"%w_bitfreq\"(bitno INTEGER PRIMARY KEY, freq INTEGER);"
			"CREATE TABLE \"%w\".\"%w_weightfreq\"(weight INTEGER PRIMARY KEY, freq INTEGER);"
			"INSERT INTO \"%w\".\"%w_node\" VALUES(1, zeroblob(%d))",
			pRDtree->zDb, pRDtree->zName, 
			pRDtree->zDb, pRDtree->zName, 
			pRDtree->zDb, pRDtree->zName, 
			pRDtree->zDb, pRDtree->zName, 
			pRDtree->zDb, pRDtree->zName, 
			pRDtree->zDb, pRDtree->zName, pRDtree->iNodeSize
			);
    if (!zCreate) {
      return SQLITE_NOMEM;
    }
    rc = sqlite3_exec(pRDtree->db, zCreate, 0, 0, 0);
    sqlite3_free(zCreate);
    if (rc != SQLITE_OK) {
      return rc;
    }
    
    char *zInitBitfreq
      = sqlite3_mprintf("INSERT INTO \"%w\".\"%w_bitfreq\" VALUES(?, 0)",
			pRDtree->zDb, pRDtree->zName
			);
    if (!zInitBitfreq) {
      return SQLITE_NOMEM;
    }
    sqlite3_stmt * initBitfreqStmt = 0;
    rc = sqlite3_prepare_v2(pRDtree->db, zInitBitfreq, -1, &initBitfreqStmt, 0);
    sqlite3_free(zInitBitfreq);
    if (rc != SQLITE_OK) {
      return rc;
    }

    for (i=0; i < pRDtree->iBfpSize*8; ++i) {
      rc = sqlite3_bind_int(initBitfreqStmt, 1, i);
      if (rc != SQLITE_OK) {
        break;
      }
      rc = sqlite3_step(initBitfreqStmt);
      if (rc != SQLITE_DONE) {
        break;
      }
      else {
        /* reassign the rc status and keep the error handling simple */
        rc = SQLITE_OK; 
      }
      sqlite3_reset(initBitfreqStmt);
    }
    sqlite3_finalize(initBitfreqStmt);
    if (rc != SQLITE_OK) {
      return rc;
    }

    char *zInitWeightfreq
      = sqlite3_mprintf("INSERT INTO \"%w\".\"%w_weightfreq\" VALUES(?, 0)",
			pRDtree->zDb, pRDtree->zName
			);
    if (!zInitWeightfreq) {
      return SQLITE_NOMEM;
    }
    sqlite3_stmt * initWeightfreqStmt = 0;
    rc = sqlite3_prepare_v2(pRDtree->db, zInitWeightfreq, -1, &initWeightfreqStmt, 0);
    sqlite3_free(zInitWeightfreq);
    if (rc != SQLITE_OK) {
      return rc;
    }

    for (i=0; i <= pRDtree->iBfpSize*8; ++i) {
      rc = sqlite3_bind_int(initWeightfreqStmt, 1, i);
      if (rc != SQLITE_OK) {
        break;
      }
      rc = sqlite3_step(initWeightfreqStmt);
      
      if (rc != SQLITE_DONE) {
        break;
      }
      else {
        /* reassign the rc status and keep the error handling simple */
        rc = SQLITE_OK; 
      }
      sqlite3_reset(initWeightfreqStmt);
    }
    sqlite3_finalize(initWeightfreqStmt);
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
  appStmt[9] = &pRDtree->pIncrementBitfreq;
  appStmt[10] = &pRDtree->pDecrementBitfreq;
  appStmt[11] = &pRDtree->pIncrementWeightfreq;
  appStmt[12] = &pRDtree->pDecrementWeightfreq;
  
  for (i=0; i<N_STATEMENT && rc==SQLITE_OK; i++) {
    char *zSql = sqlite3_mprintf(azSql[i], pRDtree->zDb, pRDtree->zName);
    if (zSql) {
      rc = sqlite3_prepare_v3(pRDtree->db, zSql, -1, SQLITE_PREPARE_PERSISTENT, appStmt[i], 0);
    }
    else {
      rc = SQLITE_NOMEM;
    }
    sqlite3_free(zSql);
  }
#endif
  return rc;
}

void RDtreeVtab::incref()
{
  ++nBusy;
}

void RDtreeVtab::decref()
{
  --nBusy;
  if (nBusy == 0) {
    /*sqlite3_finalize(pRDtree->pReadNode);
    sqlite3_finalize(pRDtree->pWriteNode);
    sqlite3_finalize(pRDtree->pDeleteNode);
    sqlite3_finalize(pRDtree->pReadRowid);
    sqlite3_finalize(pRDtree->pWriteRowid);
    sqlite3_finalize(pRDtree->pDeleteRowid);
    sqlite3_finalize(pRDtree->pReadParent);
    sqlite3_finalize(pRDtree->pWriteParent);
    sqlite3_finalize(pRDtree->pDeleteParent);
    sqlite3_finalize(pRDtree->pIncrementBitfreq);
    sqlite3_finalize(pRDtree->pDecrementBitfreq);
    sqlite3_finalize(pRDtree->pIncrementWeightfreq);
    sqlite3_finalize(pRDtree->pDecrementWeightfreq);*/
    delete this; /* !!! */
  }
}