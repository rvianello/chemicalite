#include <assert.h>
#include <string.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "chemicalite.h"
#include "rdkit_adapter.h"
#include "utils.h"
#include "object.h"
#include "bitstring.h"

int chemicalite_init_bitstring(sqlite3 *db)
{
  int rc = SQLITE_OK;
  return rc;
}
