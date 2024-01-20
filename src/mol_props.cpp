#include <cassert>
#include <cstring>
#include <string>
#include <memory>
#include <vector>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/ROMol.h>

#include "utils.hpp"
#include "mol_props.hpp"
#include "mol.hpp"
#include "logging.hpp"

// static const int MOL_PROPERTIES_PROPERTY_COLUMN = 0;
static const int MOL_PROPERTIES_MOLECULE_COLUMN = 1;

static int molPropsConnect(sqlite3 *db, void */*pAux*/,
                      int /*argc*/, const char * const */*argv*/,
                      sqlite3_vtab **ppVTab,
                      char **pzErr)
{
  int rc = sqlite3_declare_vtab(db, "CREATE TABLE x("
    "property TEXT, "
    "molecule HIDDEN"
    ")");

  if (rc == SQLITE_OK) {
    sqlite3_vtab *vtab = *ppVTab = (sqlite3_vtab *) sqlite3_malloc(sizeof(sqlite3_vtab));
    if (!vtab) {
      rc = SQLITE_NOMEM;
    }
    else {
      memset(vtab, 0, sizeof(sqlite3_vtab));
    }
  }

  if (rc != SQLITE_OK) {
    *pzErr = sqlite3_mprintf("%s", sqlite3_errmsg(db));
  }

  return rc;
}

static int molPropsBestIndex(sqlite3_vtab */*pVTab*/, sqlite3_index_info *pIndexInfo)
{
  // At this time the only supported arg is the input molecule
  // this function is consequently very simple. it might become more complex
  // when/if support for additional args/options is implemented. 
  int molecule_index = -1;
  for (int index = 0; index < pIndexInfo->nConstraint; ++index) {
    if (pIndexInfo->aConstraint[index].usable == 0) {
      continue;
    }
    if (pIndexInfo->aConstraint[index].iColumn == MOL_PROPERTIES_MOLECULE_COLUMN &&
        pIndexInfo->aConstraint[index].op == SQLITE_INDEX_CONSTRAINT_EQ) {
      molecule_index = index;
    }
  }
  if (molecule_index < 0) {
    // The input molecule is not available, or it's not usable,
    // This plan is therefore unusable.
    return SQLITE_CONSTRAINT;
  }
  pIndexInfo->idxNum = 1; // Not really meaningful a this time
  pIndexInfo->aConstraintUsage[molecule_index].argvIndex = 1; // will be argv[0] for xFilter
  pIndexInfo->aConstraintUsage[molecule_index].omit = 1; // no need for SQLite to verify
  pIndexInfo->estimatedCost = 10000; // this function should be actually very cheap
  return SQLITE_OK;
}

static int molPropsDisconnect(sqlite3_vtab *pVTab)
{
  sqlite3_free(pVTab);
  return SQLITE_OK;
}

struct MolPropsCursor : public sqlite3_vtab_cursor {
  uint32_t index;
  std::vector<std::string> props;
};

static int molPropsOpen(sqlite3_vtab */*pVTab*/, sqlite3_vtab_cursor **ppCursor)
{
  int rc = SQLITE_OK;
  MolPropsCursor *pCsr = new MolPropsCursor;
  *ppCursor = (sqlite3_vtab_cursor *)pCsr;
  return rc;
}

static int molPropsClose(sqlite3_vtab_cursor *pCursor)
{
  MolPropsCursor *p = (MolPropsCursor *)pCursor;
  delete p;
  return SQLITE_OK;
}

static int molPropsFilter(sqlite3_vtab_cursor *pCursor, int /*idxNum*/, const char */*idxStr*/,
                     int argc, sqlite3_value **argv)
{
  MolPropsCursor *p = (MolPropsCursor *)pCursor;

  if (argc != 1) {
    // at present we always expect the input file path as the only arg
    return SQLITE_ERROR;
  }

  sqlite3_value *arg = argv[0];

  int rc = SQLITE_OK;
  std::unique_ptr<RDKit::ROMol> mol(arg_to_romol(arg, &rc));

  if (rc != SQLITE_OK) {
    return rc;
  }

  p->index = 0;
  p->props.clear();

  if (mol) {
      for (const auto & property: mol->getPropList()) {
        p->props.push_back(property);
      }
  }
  else {
      // error? or do nothing?
  }

  return SQLITE_OK;
}

static int molPropsNext(sqlite3_vtab_cursor *pCursor)
{
  MolPropsCursor * p = (MolPropsCursor *)pCursor;
  p->index += 1;
  return SQLITE_OK;
}

static int molPropsEof(sqlite3_vtab_cursor *pCursor)
{
  MolPropsCursor * p = (MolPropsCursor *)pCursor;
  return p->index >= p->props.size() ? 1 : 0;
}

static int molPropsColumn(sqlite3_vtab_cursor *pCursor, sqlite3_context *ctx, int N)
{
  MolPropsCursor * p = (MolPropsCursor *)pCursor;

  switch (N) {
    case 0:
      // the property name
      assert(p->index < p->props.size());
      sqlite3_result_text(ctx, p->props[p->index].c_str(), -1, SQLITE_TRANSIENT);
      break;
    default:
      assert(!"unexpected column number");
      sqlite3_result_null(ctx);
  }
  return SQLITE_OK;
}

static int molPropsRowid(sqlite3_vtab_cursor *pCursor, sqlite_int64 *pRowid)
{
  MolPropsCursor * p = (MolPropsCursor *)pCursor;
  *pRowid = p->index + 1;
  return SQLITE_OK;
}

/*
** The mol props module, collecting the methods that implement mol_prop_list as an eponymous virtual table
*/
static sqlite3_module molPropsModule = {
  0,                           /* iVersion */
  0,                           /* xCreate - create a table */ /* null because eponymous-only */
  molPropsConnect,             /* xConnect - connect to an existing table */
  molPropsBestIndex,           /* xBestIndex - Determine search strategy */
  molPropsDisconnect,          /* xDisconnect - Disconnect from a table */
  0,                           /* xDestroy - Drop a table */
  molPropsOpen,                /* xOpen - open a cursor */
  molPropsClose,               /* xClose - close a cursor */
  molPropsFilter,              /* xFilter - configure scan constraints */
  molPropsNext,                /* xNext - advance a cursor */
  molPropsEof,                 /* xEof */
  molPropsColumn,              /* xColumn - read data */
  molPropsRowid,               /* xRowid - read data */
  0,                           /* xUpdate - write data */
  0,                           /* xBegin - begin transaction */
  0,                           /* xSync - sync transaction */
  0,                           /* xCommit - commit transaction */
  0,                           /* xRollback - rollback transaction */
  0,                           /* xFindFunction - function overloading */
  0,                           /* xRename - rename the table */
  0,                           /* xSavepoint */
  0,                           /* xRelease */
  0,                           /* xRollbackTo */
  0                            /* xShadowName */
};

static void mol_set_prop(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  int rc = SQLITE_OK;
  sqlite3_value *arg = nullptr;
  
  // the input molecule
  arg = argv[0];
  std::unique_ptr<RDKit::ROMol> mol(arg_to_romol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  // the property name
  arg = argv[1];
  if (sqlite3_value_type(arg) != SQLITE_TEXT) {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    chemicalite_log(SQLITE_MISMATCH, "the property key arg must be of type text or NULL");
    return;
  }
  std::string key = (const char *) sqlite3_value_text(arg);

  // the property value
  arg = argv[2];
  if (sqlite3_value_type(arg) == SQLITE_TEXT) {
    std::string value = (const char *) sqlite3_value_text(arg);
    mol->setProp(key, value);
  }
  else if (sqlite3_value_type(arg) == SQLITE_INTEGER) {
    int value = sqlite3_value_int(arg);
    mol->setProp(key, value);
  }
  else if (sqlite3_value_type(arg) == SQLITE_FLOAT) {
    double value = sqlite3_value_double(arg);
    mol->setProp(key, value);
  }
  else {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    chemicalite_log(SQLITE_MISMATCH, "the property value arg must be of type text, int, real or NULL");
    return;
  }

  Blob blob = mol_to_blob(*mol, &rc);
  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
  }
}

static void mol_has_prop(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  int rc = SQLITE_OK;
  sqlite3_value *arg = nullptr;
  
  // the input molecule
  arg = argv[0];
  std::unique_ptr<RDKit::ROMol> mol(arg_to_romol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  // the property name
  arg = argv[1];
  if (sqlite3_value_type(arg) != SQLITE_TEXT) {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    chemicalite_log(SQLITE_MISMATCH, "the property key arg must be of type text or NULL");
    return;
  }
  std::string key = (const char *) sqlite3_value_text(arg);

  // does this mol have this prop?
  bool it_does = mol->hasProp(key);
  sqlite3_result_int(ctx, it_does ? 1 : 0);
}

template <void (*F)(sqlite3_context*, const RDKit::ROMol &, const std::string &)>
static void mol_get_prop(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  int rc = SQLITE_OK;
  sqlite3_value *arg = nullptr;
  
  // the input molecule
  arg = argv[0];
  std::unique_ptr<RDKit::ROMol> mol(arg_to_romol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  // the property name
  arg = argv[1];
  if (sqlite3_value_type(arg) != SQLITE_TEXT) {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    chemicalite_log(SQLITE_MISMATCH, "the property key arg must be of type text or NULL");
    return;
  }
  std::string key = (const char *) sqlite3_value_text(arg);

  // does this mol have this prop?
  bool it_does = mol->hasProp(key);

  if (it_does) {
    try {
      F(ctx, *mol, key);
    }
    catch (const std::bad_any_cast & e) {
      sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    }
  }
  else {
    sqlite3_result_null(ctx);
  }
}

static void mol_get_text_prop(sqlite3_context* ctx, const RDKit::ROMol & mol, const std::string & key)
{
  std::string value;
  mol.getProp(key, value);
  sqlite3_result_text(ctx, value.c_str(), -2, SQLITE_TRANSIENT);
}

static void mol_get_int_prop(sqlite3_context* ctx, const RDKit::ROMol & mol, const std::string & key)
{
  int value;
  mol.getProp(key, value);
  sqlite3_result_int(ctx, value);
}

static void mol_get_float_prop(sqlite3_context* ctx, const RDKit::ROMol & mol, const std::string & key)
{
  double value;
  mol.getProp(key, value);
  sqlite3_result_double(ctx, value);
}

int chemicalite_init_mol_props(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) {
    rc = sqlite3_create_module_v2(db, "mol_prop_list", &molPropsModule, 
				  0,  /* Client data for xCreate/xConnect */
				  0   /* Module destructor function */
				  );
  }

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_set_prop", 3, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_set_prop>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_has_prop", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_has_prop>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_get_text_prop", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_get_prop<mol_get_text_prop>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_get_int_prop", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_get_prop<mol_get_int_prop>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_get_float_prop", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_get_prop<mol_get_float_prop>>, 0, 0);

  return rc;
}
