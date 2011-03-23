#include <cassert>

#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

/*
** Register the chemicalite module with database handle db.
*/
extern "C" int sqlite3_chemicalite_init(sqlite3 *db)
{
  int rc = SQLITE_OK;
  
  /*
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, [...]);
  }
  */

  /*
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_module_v2(db, [...]);
  }
  */

  return rc;
}

extern "C" int sqlite3_extension_init(sqlite3 *db, char **pzErrMsg,
				      const sqlite3_api_routines *pApi)
{
  SQLITE_EXTENSION_INIT2(pApi)
  return sqlite3_chemicalite_init(db);
}
