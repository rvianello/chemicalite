#include <utility>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/ROMol.h>
#include <GraphMol/Descriptors/MolDescriptors.h>

#include "utils.hpp"
#include "mol_descriptors.hpp"
#include "mol.hpp"


template <typename F, F f>
static void mol_descriptor(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  sqlite3_value *arg = argv[0];

  int rc = SQLITE_OK;
  std::unique_ptr<RDKit::ROMol> mol(arg_to_romol(arg, &rc));

  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    typename std::result_of<F(const RDKit::ROMol &)>::type descriptor = f(*mol);
    sqlite3_result(ctx, descriptor);
  }
}

static double mol_amw(const RDKit::ROMol & mol)
{
  return RDKit::Descriptors::calcAMW(mol);
}

#define MOL_DESCRIPTOR(func) mol_descriptor<decltype(&func), &func>

int chemicalite_init_mol_descriptors(sqlite3 *db)
{
  int rc = SQLITE_OK;
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_amw", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_amw)>, 0, 0);
  return rc;
}
