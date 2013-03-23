#include <stdio.h>
#include <string.h>
#include "testcommon.h"

int database_setup(const char * dbname, const char * extension, 
		   sqlite3 **pDb, char **pErrMsg)
{
  *pErrMsg = 0;
  *pDb = 0;

  int rc = sqlite3_open(dbname, pDb);

  if (SQLITE_OK == rc && extension) {
    rc = sqlite3_enable_load_extension(*pDb, 1);

    if (SQLITE_OK == rc) {
      rc = sqlite3_load_extension(*pDb, extension, 0, pErrMsg);
    }
  }

  return rc;
}

int create_rdtree(sqlite3 *db, const char * name, int len, char **pErrMsg)
{
  char sql[1024];
  sprintf(sql, 
	  "CREATE VIRTUAL TABLE %s USING rdtree("
	  "id integer primary key, s bytes(%d)"
	  ");", name, len);
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

