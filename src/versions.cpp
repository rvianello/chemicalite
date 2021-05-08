#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <RDGeneral/versions.h>

#include "versions.hpp"
#include "utils.hpp"

/*
** Return the version info for this extension
*/
static void chemicalite_version(sqlite3_context* ctx, int /*argc*/, sqlite3_value** /*argv*/)
{
  sqlite3_result_text(ctx, XSTRINGIFY(CHEMICALITE_VERSION), -1, SQLITE_STATIC);
}

static void rdkit_version(sqlite3_context* ctx, int /*argc*/, sqlite3_value** /*argv*/)
{
  sqlite3_result_text(ctx, RDKit::rdkitVersion, -1, SQLITE_STATIC);
}

static void rdkit_build(sqlite3_context* ctx, int /*argc*/, sqlite3_value** /*argv*/)
{
  sqlite3_result_text(ctx, RDKit::rdkitBuild, -1, SQLITE_STATIC);
}

static void boost_version(sqlite3_context* ctx, int /*argc*/, sqlite3_value** /*argv*/)
{
  sqlite3_result_text(ctx, RDKit::boostVersion, -1, SQLITE_STATIC);
}

int chemicalite_init_versions(sqlite3 *db)
{
  int rc = SQLITE_OK;
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "chemicalite_version", 0, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, chemicalite_version, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "rdkit_version", 0, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, rdkit_version, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "rdkit_build", 0, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, rdkit_build, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "boost_version", 0, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, boost_version, 0, 0);
  return rc;
}
