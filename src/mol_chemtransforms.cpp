#include <memory>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/ChemTransforms/ChemTransforms.h>

#include "utils.hpp"
#include "rowsvecvtab.hpp"
#include "mol_chemtransforms.hpp"
#include "mol.hpp"

static void mol_delete_substructs(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
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

  // the input pattern
  arg = argv[1];
  std::unique_ptr<RDKit::ROMol> query(arg_to_romol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  std::unique_ptr<RDKit::ROMol> result(RDKit::deleteSubstructs(*mol, *query));

  Blob blob = mol_to_blob(*result, &rc);
  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
  }
}

static int molReplSubstructsConnect(sqlite3 *db, void */*pAux*/,
                      int /*argc*/, const char * const */*argv*/,
                      sqlite3_vtab **ppVTab,
                      char **pzErr)
{
  int rc = sqlite3_declare_vtab(db, "CREATE TABLE x("
    "result MOL, "
    "molecule HIDDEN, "
    "query HIDDEN, "
    "replacement HIDDEN"
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

static const int MOL_REPL_SUBSTRUCTS_MOLECULE_COLUMN = 1;
static const int MOL_REPL_SUBSTRUCTS_QUERY_COLUMN = 2;
static const int MOL_REPL_SUBSTRUCTS_REPLACEMENT_COLUMN = 3;

int molReplSubstructsBestIndex(sqlite3_vtab */*pVTab*/, sqlite3_index_info *pIndexInfo)
{
  // At this time the only supported arg is the input molecule
  // this function is consequently very simple. it might become more complex
  // when/if support for additional args/options is implemented. 
  int molecule_index = -1;
  int query_index = -1;
  int replacement_index = -1;
  for (int index = 0; index < pIndexInfo->nConstraint; ++index) {
    if (pIndexInfo->aConstraint[index].usable == 0) {
      continue;
    }
    if (pIndexInfo->aConstraint[index].op != SQLITE_INDEX_CONSTRAINT_EQ) {
      continue;
    }
    if (pIndexInfo->aConstraint[index].iColumn == MOL_REPL_SUBSTRUCTS_MOLECULE_COLUMN) {
      molecule_index = index;
    }
    else if (pIndexInfo->aConstraint[index].iColumn == MOL_REPL_SUBSTRUCTS_QUERY_COLUMN) {
      query_index = index;
    }
    else if (pIndexInfo->aConstraint[index].iColumn == MOL_REPL_SUBSTRUCTS_REPLACEMENT_COLUMN) {
      replacement_index = index;
    }
  }
  if (molecule_index < 0 || query_index < 0 || replacement_index < 0) {
    // Some input args are not available, or not usable,
    // This plan is therefore unusable.
    return SQLITE_CONSTRAINT;
  }
  pIndexInfo->idxNum = 1; // Not really meaningful a this time
  pIndexInfo->aConstraintUsage[molecule_index].argvIndex = 1; // will be argv[0] for xFilter
  pIndexInfo->aConstraintUsage[molecule_index].omit = 1; // no need for SQLite to verify
  pIndexInfo->aConstraintUsage[query_index].argvIndex = 2;
  pIndexInfo->aConstraintUsage[query_index].omit = 1;
  pIndexInfo->aConstraintUsage[replacement_index].argvIndex = 3;
  pIndexInfo->aConstraintUsage[replacement_index].omit = 1;
  pIndexInfo->estimatedCost = 10000; // this function should be actually very cheap
  return SQLITE_OK;
}

static int molReplSubstructsDisconnect(sqlite3_vtab *pVTab)
{
  sqlite3_free(pVTab);
  return SQLITE_OK;
}

using MolRowsCursor = RowsVecCursor<RDKit::ROMOL_SPTR>;

static int molReplSubstructsFilter(sqlite3_vtab_cursor *pCursor, int /*idxNum*/, const char */*idxStr*/,
                     int argc, sqlite3_value **argv)
{
  MolRowsCursor *p = (MolRowsCursor *)pCursor;

  if (argc != 3) {
    return SQLITE_ERROR;
  }

  int rc = SQLITE_OK;
  sqlite3_value *arg = nullptr;
  
  // the input molecule
  arg = argv[0];
  std::unique_ptr<RDKit::ROMol> molecule(arg_to_romol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    return rc;
  }

  // the input pattern
  arg = argv[1];
  std::unique_ptr<RDKit::ROMol> query(arg_to_romol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    return rc;
  }

  // the replacement
  arg = argv[2];
  std::unique_ptr<RDKit::ROMol> replacement(arg_to_romol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    return rc;
  }

  p->index = 0;
  p->rows = RDKit::replaceSubstructs(*molecule, *query, *replacement);

  return SQLITE_OK;
}

static int molReplSubstructsColumn(sqlite3_vtab_cursor *pCursor, sqlite3_context *ctx, int N)
{
  MolRowsCursor * p = (MolRowsCursor *)pCursor;

  if (N == 0) {
    int rc = SQLITE_OK;
    Blob blob = mol_to_blob(*p->rows[p->index], &rc);
    if (rc != SQLITE_OK) {
      sqlite3_result_error_code(ctx, rc);
    }
    else {
      sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
    }
  }
  else {
    assert(!"unexpected column number");
    sqlite3_result_null(ctx);
  }
  return SQLITE_OK;
}


/*
** The mol replace substructs module
*/
static sqlite3_module molReplSubstructsModule = {
  0,                           /* iVersion */
  0,                           /* xCreate - create a table */ /* null because eponymous-only */
  molReplSubstructsConnect,    /* xConnect - connect to an existing table */
  molReplSubstructsBestIndex,  /* xBestIndex - Determine search strategy */
  molReplSubstructsDisconnect, /* xDisconnect - Disconnect from a table */
  0,                           /* xDestroy - Drop a table */
  rowsVecOpen<MolRowsCursor>,  /* xOpen - open a cursor */
  rowsVecClose<MolRowsCursor>, /* xClose - close a cursor */
  molReplSubstructsFilter,     /* xFilter - configure scan constraints */
  rowsVecNext<MolRowsCursor>,  /* xNext - advance a cursor */
  rowsVecEof<MolRowsCursor>,   /* xEof */
  molReplSubstructsColumn,     /* xColumn - read data */
  rowsVecRowid<MolRowsCursor>, /* xRowid - read data */
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

static void mol_replace_sidechains(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
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

  // the input pattern
  arg = argv[1];
  std::unique_ptr<RDKit::ROMol> query(arg_to_romol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  std::unique_ptr<RDKit::ROMol> result(RDKit::replaceSidechains(*mol, *query));

  Blob blob = mol_to_blob(*result, &rc);
  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
  }
}

static void mol_replace_core(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
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

  // the input pattern
  arg = argv[1];
  std::unique_ptr<RDKit::ROMol> query(arg_to_romol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  std::unique_ptr<RDKit::ROMol> result(RDKit::replaceCore(*mol, *query));

  Blob blob = mol_to_blob(*result, &rc);
  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
  }
}

static void mol_murcko_decompose(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
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

  std::unique_ptr<RDKit::ROMol> result(RDKit::MurckoDecompose(*mol));

  Blob blob = mol_to_blob(*result, &rc);
  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
  }
}

int chemicalite_init_mol_chemtransforms(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_delete_substructs", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_delete_substructs>, 0, 0);
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_module_v2(db, "mol_replace_substructs", &molReplSubstructsModule, 
				  0,  /* Client data for xCreate/xConnect */
				  0   /* Module destructor function */
				  );
  }
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_replace_sidechains", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_replace_sidechains>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_replace_core", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_replace_core>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_murcko_decompose", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_murcko_decompose>, 0, 0);

  return rc;
}
