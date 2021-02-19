#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <RDGeneral/versions.h>

#include "versions.hpp"
#include "utils.hpp"

/*
** Return the version info for this extension
*/
static void chemicalite_version_f(sqlite3_context* ctx, int /*argc*/, sqlite3_value** /*argv*/)
{
  sqlite3_result_text(ctx, XSTRINGIFY(CHEMICALITE_VERSION), -1, SQLITE_STATIC);
}

static void rdkit_version_f(sqlite3_context* ctx, int /*argc*/, sqlite3_value** /*argv*/)
{
  sqlite3_result_text(ctx, RDKit::rdkitVersion, -1, SQLITE_STATIC);
}

static void rdkit_build_f(sqlite3_context* ctx, int /*argc*/, sqlite3_value** /*argv*/)
{
  sqlite3_result_text(ctx, RDKit::rdkitBuild, -1, SQLITE_STATIC);
}

static void boost_version_f(sqlite3_context* ctx, int /*argc*/, sqlite3_value** /*argv*/)
{
  sqlite3_result_text(ctx, RDKit::boostVersion, -1, SQLITE_STATIC);
}

int chemicalite_init_versions(sqlite3 *db)
{
  int rc = SQLITE_OK;
  CREATE_SQLITE_NULLARY_FUNCTION(chemicalite_version);
  CREATE_SQLITE_NULLARY_FUNCTION(rdkit_version);
  CREATE_SQLITE_NULLARY_FUNCTION(rdkit_build);
  CREATE_SQLITE_NULLARY_FUNCTION(boost_version);
  return rc;
}
