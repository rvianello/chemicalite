#include <cassert>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/FMCS/FMCS.h>

#include "utils.hpp"
#include "mol_fmcs.hpp"
#include "mol.hpp"

void mol_fmcs_step(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  assert(argc == 1);
  int rc = SQLITE_OK;
  
  sqlite3_value *arg = argv[0];
  RDKit::ROMOL_SPTR mol(arg_to_romol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  void **agg = (void **) sqlite3_aggregate_context(ctx, sizeof(void *));

  if (agg) {
    if (!*agg) {
      *agg = (void *) new std::vector<RDKit::ROMOL_SPTR>;
    }
    std::vector<RDKit::ROMOL_SPTR> *mols = (std::vector<RDKit::ROMOL_SPTR> *) *agg;
    mols->push_back(mol);
  }  
}

void mol_fmcs_final(sqlite3_context * ctx)
{
  void **agg = (void **) sqlite3_aggregate_context(ctx, 0);

  if (agg) {
    if (!*agg) {
      *agg = (void *) new std::vector<RDKit::ROMOL_SPTR>;
    }
    std::vector<RDKit::ROMOL_SPTR> *mols = (std::vector<RDKit::ROMOL_SPTR> *) *agg;
    RDKit::MCSResult results = findMCS(*mols);

    sqlite3_result_text(ctx, results.SmartsString.c_str(), -1, SQLITE_TRANSIENT);
  }
  else {
    sqlite3_result_null(ctx);
  }
}

int chemicalite_init_mol_fmcs(sqlite3 *db)
{
  (void)db;
  int rc = SQLITE_OK;

  rc = sqlite3_create_window_function(
    db,
    "mol_find_mcs",
    1, // int nArg,
    SQLITE_UTF8, // int eTextRep,
    0, // void *pApp,
    mol_fmcs_step, // void (*xStep)(sqlite3_context*,int,sqlite3_value**),
    mol_fmcs_final, // void (*xFinal)(sqlite3_context*),
    0, // void (*xValue)(sqlite3_context*),
    0, // void (*xInverse)(sqlite3_context*,int,sqlite3_value**),
    0  // void(*xDestroy)(void*)
  );

  return rc;
}
