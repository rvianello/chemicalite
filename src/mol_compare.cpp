#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/ROMol.h>
#include <GraphMol/Descriptors/MolDescriptors.h>
#include <GraphMol/Substruct/SubstructMatch.h>
#include <GraphMol/SmilesParse/SmilesWrite.h>

#include "mol_compare.hpp"
#include "mol.hpp"
#include "utils.hpp"

static void free_romol_auxdata(void * aux)
{
  delete (RDKit::ROMol *) aux;
}

int mol_is_substruct(const RDKit::ROMol & m1, const RDKit::ROMol & m2)
{
  RDKit::MatchVectType matchVect;
  bool recursion_possible = true;
  bool do_chiral_match = false; /* FIXME: make configurable getDoChiralSSS(); */
  return RDKit::SubstructMatch(m1, m2, matchVect, recursion_possible, do_chiral_match) ? 1 : 0; 
}

int mol_is_superstruct(const RDKit::ROMol & m1, const RDKit::ROMol & m2)
{
  return mol_is_substruct(m2, m1);
}

int mol_cmp(const RDKit::ROMol & m1, const RDKit::ROMol & m2)
{
  int res = m1.getNumAtoms() - m2.getNumAtoms();
  if (res) {return (res > 0) ? 1 : -1;}

  res = m1.getNumBonds() - m2.getNumBonds();
  if (res) {return (res > 0) ? 1 : -1;}

  res = int(RDKit::Descriptors::calcAMW(m1, false) -
	    RDKit::Descriptors::calcAMW(m2, false) + .5);
  if (res) {return (res > 0) ? 1 : -1;}

  res = m1.getRingInfo()->numRings() - m2.getRingInfo()->numRings();
  if (res) {return (res > 0) ? 1 : -1;}

  RDKit::MatchVectType matchVect;
  bool recursion_possible = false;
  bool do_chiral_match = false; /* FIXME: make configurable getDoChiralSSS(); */
  bool ss1 = RDKit::SubstructMatch(m1, m2, matchVect, recursion_possible,
                                   do_chiral_match);
  bool ss2 = RDKit::SubstructMatch(m2, m1, matchVect, recursion_possible,
                                   do_chiral_match);
  if (ss1 && !ss2) {
    return 1;
  } else if (!ss1 && ss2) {
    return -1;
  }

  // the above can still fail in some chirality cases
  std::string smi1 = MolToSmiles(m1, do_chiral_match);
  std::string smi2 = MolToSmiles(m2, do_chiral_match);
  return smi1 == smi2 ? 0 : (smi1 < smi2 ? -1 : 1);
}

template <int (*F)(const RDKit::ROMol &, const RDKit::ROMol &)>
static void mol_compare(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  int rc = SQLITE_OK;

  RDKit::ROMol *p1 = nullptr;
  RDKit::ROMol *p2 = nullptr;

  void * aux1 = sqlite3_get_auxdata(ctx, 0);
  if (aux1) {
    p1 = (RDKit::ROMol *) aux1;
  }
  else {
    p1 = arg_to_romol(argv[0], ctx, &rc);
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
    p2 = arg_to_romol(argv[1], ctx, &rc);
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

int chemicalite_init_mol_compare(sqlite3 *db)
{
  int rc = SQLITE_OK;
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_is_substruct", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_compare<mol_is_substruct>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_is_superstruct", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_compare<mol_is_superstruct>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_cmp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_compare<mol_cmp>>, 0, 0);
  return rc;
}

