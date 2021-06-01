#include <memory>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/ChemTransforms/ChemTransforms.h>

#include "utils.hpp"
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

#if 0
static void mol_replace_substructs(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
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

  // the replacement
  arg = argv[2];
  std::unique_ptr<RDKit::ROMol> replacement(arg_to_romol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  std::unique_ptr<RDKit::ROMol> result(RDKit::replaceSubstructs(*mol, *query, *replacement));

  Blob blob = mol_to_blob(*result, &rc);
  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
  }
}
#endif

int chemicalite_init_mol_chemtransforms(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_delete_substructs", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_delete_substructs>, 0, 0);
#if 0
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_replace_substructs", 3, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_replace_substructs>, 0, 0);
#endif
  return rc;
}
