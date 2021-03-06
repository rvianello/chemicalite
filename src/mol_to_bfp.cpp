#include <memory>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/Fingerprints/Fingerprints.h>
#include <GraphMol/Fingerprints/AtomPairs.h>
#include <GraphMol/Fingerprints/MorganFingerprints.h>
#include <GraphMol/Fingerprints/MACCS.h>
#include <DataStructs/ExplicitBitVect.h>
#include <DataStructs/BitOps.h>

#include "utils.hpp"
#include "mol.hpp"
#include "bfp.hpp"
//#include "mol_to_bfp.hpp"

template <ExplicitBitVect * (*F)(const RDKit::ROMol &, int), int DEFAULT_LENGTH>
static void mol_to_bfp(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  int rc = SQLITE_OK;

  std::string * pbfp = 0;

  void * aux = sqlite3_get_auxdata(ctx, 0);
  if (aux) {
    pbfp = (std::string *) aux;
  }
  else {
    std::unique_ptr<RDKit::ROMol> pmol(arg_to_romol(argv[0], &rc));

    if (rc == SQLITE_OK && argc > 1 && sqlite3_value_type(argv[1]) != SQLITE_INTEGER) {
      rc = SQLITE_MISMATCH;
    }

    if (rc == SQLITE_OK && pmol) {
      int length = (argc > 1) ? sqlite3_value_int(argv[1]) : DEFAULT_LENGTH;
      try {
        std::unique_ptr<ExplicitBitVect> bv(F(*pmol, length));
        if (bv) {
          pbfp = new std::string(BitVectToBinaryText(*bv));
        }
        else {
          rc = SQLITE_ERROR;
        }
      } 
      catch (...) {
        // unknown exception
        rc = SQLITE_ERROR;
      }
    }

    if (rc == SQLITE_OK) {
      sqlite3_set_auxdata(ctx, 0, (void *) pbfp, free_bfp_auxdata);
    }
  }

  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  if (!pbfp) {
    sqlite3_result_null(ctx);
    return;
  }

  Blob blob = bfp_to_blob(*pbfp, &rc);

  if (rc == SQLITE_OK) {
    sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
  }
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

static ExplicitBitVect * mol_layered_bfp(const RDKit::ROMol & mol, int length)
{
  return RDKit::LayeredFingerprintMol(mol, 0xFFFFFFFF, 1, 7, length);
}

static ExplicitBitVect * mol_rdkit_bfp(const RDKit::ROMol & mol, int length)
{
  return RDKit::RDKFingerprintMol(mol, 1, 6, length, 2);
}

static ExplicitBitVect * mol_atom_pairs_bfp(const RDKit::ROMol & mol, int length)
{
  return RDKit::AtomPairs::getHashedAtomPairFingerprintAsBitVect(mol, length);
}

static ExplicitBitVect * mol_topological_torsion_bfp(const RDKit::ROMol & mol, int length)
{
  return RDKit::AtomPairs::getHashedTopologicalTorsionFingerprintAsBitVect(mol, length);
}

static ExplicitBitVect * mol_maccs_bfp(const RDKit::ROMol & mol, int /* unused length */)
{
  return RDKit::MACCSFingerprints::getFingerprintAsBitVect(mol);
}

static ExplicitBitVect * mol_pattern_bfp(const RDKit::ROMol & mol, int length)
{
  return RDKit::PatternFingerprintMol(mol, length);
}

template <ExplicitBitVect * (*F)(const RDKit::ROMol &, int, int), int DEFAULT_LENGTH>
static void mol_to_morgan_bfp(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  int rc = SQLITE_OK;

  std::string * pbfp = 0;

  void * aux = sqlite3_get_auxdata(ctx, 0);
  if (aux) {
    pbfp = (std::string *) aux;
  }
  else {
    std::unique_ptr<RDKit::ROMol> pmol(arg_to_romol(argv[0], &rc));

    if (rc == SQLITE_OK && sqlite3_value_type(argv[1]) != SQLITE_INTEGER) {
      rc = SQLITE_MISMATCH;
    }

    if (rc == SQLITE_OK && argc > 2 && sqlite3_value_type(argv[2]) != SQLITE_INTEGER) {
      rc = SQLITE_MISMATCH;
    }

    if (rc == SQLITE_OK && pmol) {
      int radius = sqlite3_value_int(argv[1]);
      int length = (argc > 2) ? sqlite3_value_int(argv[2]) : DEFAULT_LENGTH;
      try {
        std::unique_ptr<ExplicitBitVect> bv(F(*pmol, radius, length));
        if (bv) {
          pbfp = new std::string(BitVectToBinaryText(*bv));
        }
        else {
          rc = SQLITE_ERROR;
        }
      } 
      catch (...) {
        // unknown exception
        rc = SQLITE_ERROR;
      }
    }

    if (rc == SQLITE_OK) {
      sqlite3_set_auxdata(ctx, 0, (void *) pbfp, free_bfp_auxdata);
    }
  }

  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  if (!pbfp) {
    sqlite3_result_null(ctx);
    return;
  }

  Blob blob = bfp_to_blob(*pbfp, &rc);

  if (rc == SQLITE_OK) {
    sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
  }
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

static ExplicitBitVect * mol_morgan_bfp(const RDKit::ROMol & mol, int radius, int length)
{
  std::vector<uint32_t> invars(mol.getNumAtoms());
  RDKit::MorganFingerprints::getConnectivityInvariants(mol, invars, true);
  return RDKit::MorganFingerprints::getFingerprintAsBitVect(mol, radius, length, &invars);
}

static ExplicitBitVect * mol_feat_morgan_bfp(const RDKit::ROMol & mol, int radius, int length)
{
  std::vector<uint32_t> invars(mol.getNumAtoms());
  RDKit::MorganFingerprints::getFeatureInvariants(mol, invars);
  return RDKit::MorganFingerprints::getFingerprintAsBitVect(mol, radius, length, &invars);
}

static constexpr const int DEFAULT_SSS_BFP_LENGTH = 2048;
static constexpr const int DEFAULT_LAYERED_BFP_LENGTH = 1024;
static constexpr const int DEFAULT_MORGAN_BFP_LENGTH = 512;
static constexpr const int DEFAULT_HASHED_TORSION_BFP_LENGTH = 1024;
static constexpr const int DEFAULT_HASHED_PAIR_BFP_LENGTH = 2048;

int chemicalite_init_mol_to_bfp(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_layered_bfp", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_bfp<mol_layered_bfp, DEFAULT_LAYERED_BFP_LENGTH>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_layered_bfp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_bfp<mol_layered_bfp, DEFAULT_LAYERED_BFP_LENGTH>>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_rdkit_bfp", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_bfp<mol_rdkit_bfp, DEFAULT_LAYERED_BFP_LENGTH>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_rdkit_bfp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_bfp<mol_rdkit_bfp, DEFAULT_LAYERED_BFP_LENGTH>>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_atom_pairs_bfp", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_bfp<mol_atom_pairs_bfp, DEFAULT_HASHED_PAIR_BFP_LENGTH>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_atom_pairs_bfp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_bfp<mol_atom_pairs_bfp, DEFAULT_HASHED_PAIR_BFP_LENGTH>>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_topological_torsion_bfp", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_bfp<mol_topological_torsion_bfp, DEFAULT_HASHED_TORSION_BFP_LENGTH>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_topological_torsion_bfp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_bfp<mol_topological_torsion_bfp, DEFAULT_HASHED_TORSION_BFP_LENGTH>>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_maccs_bfp", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_bfp<mol_maccs_bfp, -1>>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_pattern_bfp", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_bfp<mol_pattern_bfp, DEFAULT_SSS_BFP_LENGTH>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_pattern_bfp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_bfp<mol_pattern_bfp, DEFAULT_SSS_BFP_LENGTH>>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_morgan_bfp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_morgan_bfp<mol_morgan_bfp, DEFAULT_MORGAN_BFP_LENGTH>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_morgan_bfp", 3, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_morgan_bfp<mol_morgan_bfp, DEFAULT_MORGAN_BFP_LENGTH>>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_feat_morgan_bfp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_morgan_bfp<mol_feat_morgan_bfp, DEFAULT_MORGAN_BFP_LENGTH>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_feat_morgan_bfp", 3, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_morgan_bfp<mol_feat_morgan_bfp, DEFAULT_MORGAN_BFP_LENGTH>>, 0, 0);

  return rc;
}
