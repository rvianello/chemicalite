#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"

int chemicalite_init_molecule(sqlite3 *db)
{
  UNUSED(db);
  int rc = SQLITE_OK;
  return rc;
}
