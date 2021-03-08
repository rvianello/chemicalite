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

static double mol_amw(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcAMW(mol);}
static double mol_tpsa(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcTPSA(mol);}
static double mol_fraction_csp3(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcFractionCSP3(mol);}

static int mol_hba(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcLipinskiHBA(mol);}
static int mol_hbd(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcLipinskiHBD(mol);}
static int mol_num_rotatable_bonds(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcNumRotatableBonds(mol);}
static int mol_num_hetatms(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcNumHeteroatoms(mol);}

static int mol_num_rings(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcNumRings(mol);}
static int mol_num_aromatic_rings(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcNumAromaticRings(mol);}
static int mol_num_aliphatic_rings(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcNumAliphaticRings(mol);}
static int mol_num_saturated_rings(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcNumSaturatedRings(mol);}

static double mol_chi0v(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcChi0v(mol);}
static double mol_chi1v(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcChi1v(mol);}
static double mol_chi2v(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcChi2v(mol);}
static double mol_chi3v(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcChi3v(mol);}
static double mol_chi4v(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcChi4v(mol);}

static double mol_chi0n(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcChi0n(mol);}
static double mol_chi1n(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcChi1n(mol);}
static double mol_chi2n(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcChi2n(mol);}
static double mol_chi3n(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcChi3n(mol);}
static double mol_chi4n(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcChi4n(mol);}

static double mol_kappa1(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcKappa1(mol);}
static double mol_kappa2(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcKappa2(mol);}
static double mol_kappa3(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcKappa3(mol);}

static double mol_logp(const RDKit::ROMol & mol)
{
  double logp, mr;
  RDKit::Descriptors::calcCrippenDescriptors(mol, logp, mr);
  return logp;
}

static int mol_num_atms(const RDKit::ROMol & mol) {return mol.getNumAtoms(false);}
static int mol_num_hvyatms(const RDKit::ROMol & mol) {return mol.getNumAtoms(true);}

static std::string mol_formula(const RDKit::ROMol & mol) {return RDKit::Descriptors::calcMolFormula(mol);}


#define MOL_DESCRIPTOR(func) mol_descriptor<decltype(&func), &func>

int chemicalite_init_mol_descriptors(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_amw", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_amw)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_tpsa", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_tpsa)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_fraction_csp3", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_fraction_csp3)>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hba", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_hba)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hbd", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_hbd)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_num_rotatable_bonds", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_num_rotatable_bonds)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_num_hetatms", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_num_hetatms)>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_num_rings", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_num_rings)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_num_aromatic_rings", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_num_aromatic_rings)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_num_aliphatic_rings", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_num_aliphatic_rings)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_num_saturated_rings", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_num_saturated_rings)>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_chi0v", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_chi0v)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_chi1v", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_chi1v)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_chi2v", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_chi2v)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_chi3v", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_chi3v)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_chi4v", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_chi4v)>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_chi0n", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_chi0n)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_chi1n", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_chi1n)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_chi2n", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_chi2n)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_chi3n", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_chi3n)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_chi4n", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_chi4n)>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_kappa1", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_kappa1)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_kappa2", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_kappa2)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_kappa3", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_kappa3)>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_logp", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_logp)>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_num_atms", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_num_atms)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_num_hvyatms", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_num_hvyatms)>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_formula", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<MOL_DESCRIPTOR(mol_formula)>, 0, 0);

  return rc;
}
