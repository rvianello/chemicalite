#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/ROMol.h>

#include "mol_compare.hpp"
#include "mol.hpp"
#include "utils.hpp"

static void free_romol_auxdata(void * aux)
{
  delete (RDKit::ROMol *) aux;
}


template <int (*F)(const RDKit::ROMol &, const RDKit::ROMol &)>
static void func_f(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  int rc = SQLITE_OK;

  RDKit::ROMol *p1 = nullptr;
  RDKit::ROMol *p2 = nullptr;

  void * aux1 = sqlite3_get_auxdata(ctx, 0);
  if (aux1) {
    p1 = (RDKit::ROMol *) aux1;
  }
  else {
    p1 = arg_to_romol(argv[0], ctx, &rc).release();
    if (rc != SQLITE_OK) {
      sqlite3_result_error_code(ctx, rc);
      return;
    }
    else {
      sqlite3_set_auxdata(ctx, 0, (void *) p1, free_romol_auxdata);
    }
  }

  void * aux2 = sqlite3_get_auxdata(ctx, 1);
  if (aux2) {
    p2 = (RDKit::ROMol *) aux2;
  }
  else {
    p2 = arg_to_romol(argv[1], ctx, &rc).release();
    if (rc != SQLITE_OK) {
      sqlite3_result_error_code(ctx, rc);
      return;
    }
    else {
      sqlite3_set_auxdata(ctx, 1, (void *) p2, free_romol_auxdata);
    }
  }

  if (p1 && p2) {
    int result = F(*p1, *p2);
    sqlite3_result_int(ctx, result);
  }
  else {
    sqlite3_result_null(ctx);
  }
}

int chemicalite_init_mol_compare(sqlite3 */*db*/)
{
  int rc = SQLITE_OK;
  return rc;
}

