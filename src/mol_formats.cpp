#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/SmilesParse/SmilesParse.h>

#include "mol.hpp"
#include "mol_formats.hpp"
#include "logging.hpp"
#include "utils.hpp"

/*static void mol_to_smiles_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  
}*/

static void mol_from_smiles_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  UNUSED(argc);
  assert(argc == 1);

  sqlite3_value *arg = argv[0];
  int value_type = sqlite3_value_type(arg);

  /* NULL on NULL */
  if (value_type == SQLITE_NULL) {
    sqlite3_result_null(ctx);
    return;
  }

  /* not a string */
  if (value_type != SQLITE3_TEXT) {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    chemicalite_log(SQLITE_MISMATCH, "input arg must be of type text or NULL");
    return;
  }

  /* build the molecule binary repr from a text string */
  try {
    RDKit::ROMol * mol = RDKit::SmilesToMol((const char *)sqlite3_value_text(arg)); 

    int rc;
    std::string buf = mol_to_binary(mol, &rc);
    if (rc != SQLITE_OK) {
      sqlite3_result_error_code(ctx, rc);
    }
    else {
      sqlite3_result_blob(ctx, buf.c_str(), buf.size(), SQLITE_TRANSIENT);
    }

    delete mol;
  }
  catch (...) {
    sqlite3_result_null(ctx);
  }
}

int chemicalite_init_mol_formats(sqlite3 *db)
{
  UNUSED(db);
  int rc = SQLITE_OK;

  CREATE_SQLITE_UNARY_FUNCTION(mol_from_smiles);

  return rc;
}
