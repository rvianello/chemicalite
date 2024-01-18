#include "test_common.hpp"

void test_db_open(sqlite3 **db)
{
  int rc = SQLITE_OK;

  // Create a connection to an in-memory database
  rc = sqlite3_open(":memory:", db);
  REQUIRE(rc == SQLITE_OK);

  // Enable loading extensions
  rc = sqlite3_enable_load_extension(*db, 1);
  REQUIRE(rc == SQLITE_OK);

  // Load ChemicaLite
  rc = sqlite3_load_extension(*db, "chemicalite", 0, 0);
  REQUIRE(rc == SQLITE_OK);
}

void test_db_close(sqlite3 *db)
{
  int rc = sqlite3_close(db);
  REQUIRE(rc == SQLITE_OK);
}

void test_select_value(sqlite3 * db, const std::string & query, double expected)
{
  int rc;
  sqlite3_stmt *pStmt;

  rc = sqlite3_prepare_v2(db, query.c_str(), -1, &pStmt, 0);
  REQUIRE(rc == SQLITE_OK);

  rc = sqlite3_step(pStmt);
  REQUIRE(rc == SQLITE_ROW);

  int value_type = sqlite3_column_type(pStmt, 0);
  REQUIRE(value_type == SQLITE_FLOAT);

  double value = sqlite3_column_double(pStmt, 0);
  REQUIRE(value == Catch::Approx(expected));

  sqlite3_finalize(pStmt);
}

void test_select_value(sqlite3 * db, const std::string & query, int expected)
{
  int rc;
  sqlite3_stmt *pStmt;

  rc = sqlite3_prepare_v2(db, query.c_str(), -1, &pStmt, 0);
  REQUIRE(rc == SQLITE_OK);

  rc = sqlite3_step(pStmt);
  REQUIRE(rc == SQLITE_ROW);

  int value_type = sqlite3_column_type(pStmt, 0);
  REQUIRE(value_type == SQLITE_INTEGER);

  int value = sqlite3_column_int(pStmt, 0);
  REQUIRE(value == expected);

  sqlite3_finalize(pStmt);
}

void test_select_value(sqlite3 * db, const std::string & query, const std::string expected)
{
  int rc;
  sqlite3_stmt *pStmt;

  rc = sqlite3_prepare_v2(db, query.c_str(), -1, &pStmt, 0);
  REQUIRE(rc == SQLITE_OK);

  rc = sqlite3_step(pStmt);
  REQUIRE(rc == SQLITE_ROW);

  int value_type = sqlite3_column_type(pStmt, 0);
  REQUIRE(value_type == SQLITE_TEXT);

  std::string value { reinterpret_cast<const char *>(sqlite3_column_text(pStmt, 0)) };
  REQUIRE(value == expected);

  sqlite3_finalize(pStmt);
}

#if 0
int database_setup(const char * dbname, sqlite3 **pDb, char **pErrMsg)
{
  *pErrMsg = 0;
  *pDb = 0;

  int rc = sqlite3_open(dbname, pDb);

  if (SQLITE_OK == rc) {
    rc = sqlite3_enable_load_extension(*pDb, 1);

    if (SQLITE_OK == rc) {
      rc = sqlite3_load_extension(*pDb, "chemicalite", 0, pErrMsg);
    }
  }

  return rc;
}

int create_rdtree(sqlite3 *db, const char * name, int len, char **pErrMsg)
{
  char sql[1024];
  sprintf(sql, 
	  "CREATE VIRTUAL TABLE %s USING rdtree("
	  "id integer primary key, s bits(%d)"
	  ");", name, len);
  return sqlite3_exec(db, sql, 0, 0, pErrMsg);
}

int create_rdtree_ex(sqlite3 *db, const char * name, int len, const char * opts,
		     char **pErrMsg)
{
  char sql[1024];
  sprintf(sql, 
	  "CREATE VIRTUAL TABLE %s USING rdtree("
	  "id integer primary key, s bytes(%d), %s"
	  ");", name, len, opts);
  return sqlite3_exec(db, sql, 0, 0, pErrMsg);
}

int insert_bitstring(sqlite3 *db, const char * name, int id, void *s, int len)
{
  char sql[1024];
  if (id <= 0) {
    sprintf(sql, "INSERT INTO %s(s) VALUES(?1);", name);
  }
  else {
    sprintf(sql, "INSERT INTO %s(id, s) VALUES(%d, ?1);", name, id);
  }

  sqlite3_stmt*pStmt = 0;
  int rc = sqlite3_prepare(db, sql, -1, &pStmt, 0);

  if (SQLITE_OK == rc) {
    rc = sqlite3_bind_blob(pStmt, 1, s, len, SQLITE_STATIC);  
  }

  if (SQLITE_OK == rc) {
    int rc2 = sqlite3_step(pStmt);
    if (rc2 != SQLITE_DONE) {
      rc = rc2;
    }  
  }

  sqlite3_finalize(pStmt);

  return rc;
}

int insert_signature(sqlite3 *db, const char * name, int id, const char *smiles)
{
  char sql[1024];
  if (id <= 0) {
    sprintf(sql, "INSERT INTO %s(s) VALUES(mol_bfp_signature(?1));", name);
  }
  else {
    sprintf(sql, "INSERT INTO %s(id, s) VALUES(%d, mol_bfp_signature(?1));", 
	    name, id);
  }

  sqlite3_stmt*pStmt = 0;
  int rc = sqlite3_prepare(db, sql, -1, &pStmt, 0);

  if (SQLITE_OK == rc) {
    rc = sqlite3_bind_text(pStmt, 1, smiles, strlen(smiles), SQLITE_STATIC);  
  }

  if (SQLITE_OK == rc) {
    int rc2 = sqlite3_step(pStmt);
    if (rc2 != SQLITE_DONE) {
      rc = rc2;
    }  
  }

  sqlite3_finalize(pStmt);

  return rc;
}

int select_integer(sqlite3 *db, const char * sql, int *pInt)
{
  sqlite3_stmt *pStmt;

  int rc = sqlite3_prepare(db, sql, -1, &pStmt, 0);

  if (SQLITE_OK == rc) {
    int rc2 = sqlite3_step(pStmt);
    if (rc2 != SQLITE_ROW) {
      rc = rc2;
    }  
    else {
      *pInt = sqlite3_column_int(pStmt, 0);
    }
  }

  sqlite3_finalize(pStmt);
  return rc;
}

int select_text(sqlite3 *db, const char * sql, const char **pTxt)
{
  sqlite3_stmt *pStmt;

  int rc = sqlite3_prepare(db, sql, -1, &pStmt, 0);

  if (SQLITE_OK == rc) {
    int rc2 = sqlite3_step(pStmt);
    if (rc2 != SQLITE_ROW) {
      rc = rc2;
    }  
    else {
      /* we need to make a copy here, otherwise the call to 
      sqlite3_finalize below may (will) invalidate the value
      before we use it */
      *pTxt = strdup((const char *)sqlite3_column_text(pStmt, 0));
      if (!*pTxt) {
        rc = SQLITE_NOMEM;
      }
    }
  }

  sqlite3_finalize(pStmt);
  return rc;
}
#endif