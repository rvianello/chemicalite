#include <memory>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/MolStandardize/MolStandardize.h>

#include "utils.hpp"
#include "mol_standardize.hpp"
#include "mol.hpp"

static void mol_cleanup(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  int rc = SQLITE_OK;
  sqlite3_value *arg = nullptr;
  
  // the input molecule
  arg = argv[0];
  std::unique_ptr<RDKit::RWMol> mol_in(arg_to_rwmol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  std::unique_ptr<RDKit::RWMol> mol_out(RDKit::MolStandardize::cleanup(*mol_in));

  Blob blob = mol_to_blob(*mol_out, &rc);
  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
  }
}


int chemicalite_init_mol_standardize(sqlite3 *db)
{
  int rc = SQLITE_OK;
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_cleanup", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_cleanup>, 0, 0);
  return rc;
}