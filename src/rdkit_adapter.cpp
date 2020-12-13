#include <cassert>
#include <cstring>
#include <string>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <RDGeneral/versions.h>
#include <GraphMol/RDKitBase.h>
#include <GraphMol/MolPickler.h>
#include <GraphMol/Descriptors/MolDescriptors.h>
#include <GraphMol/SmilesParse/SmilesParse.h>
#include <GraphMol/SmilesParse/SmartsWrite.h>
#include <GraphMol/SmilesParse/SmilesWrite.h>
#include <GraphMol/Substruct/SubstructMatch.h>
#include <GraphMol/Fingerprints/Fingerprints.h>
#include <GraphMol/Fingerprints/AtomPairs.h>
#include <GraphMol/Fingerprints/MorganFingerprints.h>
#include <GraphMol/Fingerprints/MACCS.h>
#include <DataStructs/ExplicitBitVect.h>
#include <DataStructs/BitOps.h>

#include "chemicalite.h"
#include "logging.h"
#include "bfp_ops.h"
#include "rdkit_adapter.h"

struct Mol : RDKit::ROMol {
  Mol(const Mol & other) : RDKit::ROMol(other) {}
  Mol(const std::string & pickle) : RDKit::ROMol(pickle) {}
};

void free_mol(Mol *pMol)
{
  delete static_cast<RDKit::ROMol *>(pMol);
}

Mol * clone_mol(Mol *pMol)
{
  return new Mol(*pMol);
}

struct Bfp : std::string {
  Bfp() : std::string() {}
  Bfp(const Bfp & other) : std::string(other) {}
  Bfp(const std::string & other) : std::string(other) {}
  Bfp(const char* s, size_t n) : std::string(s, n) {}
};

void free_bfp(Bfp *pBfp) { delete pBfp; }

namespace {
  const unsigned int SSS_FP_SIZE            = 8*MOL_SIGNATURE_SIZE;
  const unsigned int LAYERED_FP_SIZE        = 1024;
  const unsigned int MORGAN_FP_SIZE         = 512;
  const unsigned int HASHED_TORSION_FP_SIZE = 1024;
  const unsigned int HASHED_PAIR_FP_SIZE    = 2048;
}

// SMILES/SMARTS <-> Molecule ////////////////////////////////////////////////

int txt_to_mol(const char * txt, int as_smarts, Mol **ppMol)
{
  assert(txt);
  int rc = SQLITE_OK;

  try {
    std::string data(txt);
    RDKit::ROMol *pROMol = as_smarts ?
      RDKit::SmartsToMol(data) : RDKit::SmilesToMol(data);
      *ppMol = static_cast<Mol *>(pROMol);
  } 
  catch (...) {
    // problem generating molecule from smiles
    *ppMol = 0;
    chemicalite_log(
      SQLITE_ERROR,
      "Conversion from '%s' to %s triggered an exception.",
      txt, as_smarts ? "qmol" : "mol"
      );
  }

  if (!*ppMol) {
    // parse error
    rc = SQLITE_WARNING;
    chemicalite_log(
      rc,
      "Could not convert '%s' to %s",
      txt, as_smarts ? "qmol" : "mol"
      );
  }

  return rc;
}

int mol_to_txt(Mol *pMol, int as_smarts, char **pTxt)
{
  assert(pMol);
  *pTxt = 0;
  int rc = SQLITE_OK;

  std::string txt;
  try {
    txt.assign( as_smarts ? 
		RDKit::MolToSmarts(*pMol, false) : 
		RDKit::MolToSmiles(*pMol, true) );
  } 
  catch (...) {
    // unknown exception
    rc = SQLITE_ERROR;
  }       

  if (rc == SQLITE_OK) {
    *pTxt = sqlite3_mprintf("%s", txt.c_str());
    if (!(*pTxt)) {
      rc = SQLITE_NOMEM;
    }
  }

  return rc;               
}

// Blob <-> Molecule /////////////////////////////////////////////////////////

int blob_to_mol(const u8 *pBlob, int len, Mol **ppMol)
{
  assert(pBlob);

  int rc = SQLITE_OK;

  std::string blob;
  try {
    blob.assign((const char *)pBlob, len);
    *ppMol = new Mol(blob);
  } 
  catch (...) {
    // problem generating molecule from blob data
    *ppMol = 0;
  }
  
  if (!(*ppMol)) {
    // blob data could not be parsed
    rc = SQLITE_ERROR;
  }

  return rc;
}

int mol_to_blob(Mol *pMol, u8 **ppBlob, int *pLen)
{
  assert(pMol);

  *ppBlob = 0;
  *pLen = 0;

  int rc = SQLITE_OK;
  std::string blob;
  try {
    RDKit::MolPickler::pickleMol(*pMol, blob);
  } 
  catch (...) {
    // unknown exception
    rc = SQLITE_ERROR;
  }       

  if (rc == SQLITE_OK) {
    *ppBlob = (u8 *)sqlite3_malloc(blob.size());
    if (*ppBlob) {
      memcpy(*ppBlob, blob.data(), blob.size());
      *pLen = blob.size();
    }
    else {
      rc = SQLITE_NOMEM;
    }
  }

  return rc;
}

// Blob <-> SMILES/SMARTS ////////////////////////////////////////////////////

int txt_to_blob(const char * txt, int as_smarts, u8 **pBlob, int *pLen)
{
  Mol * pMol = 0;
  int rc = txt_to_mol(txt, as_smarts, &pMol);
  if (rc == SQLITE_OK) {
    rc = mol_to_blob(pMol, pBlob, pLen);
    free_mol(pMol);
  }
  return rc;
}

int blob_to_txt(u8 *blob, int len, int as_smarts, char **pTxt)
{
  Mol * pMol = 0;
  int rc = blob_to_mol(blob, len, &pMol);
  if (rc == SQLITE_OK) {
    rc = mol_to_txt(pMol, as_smarts, pTxt);
    free_mol(pMol);
  }
  return rc;
}

// Molecules comparison //////////////////////////////////////////////////////

int mol_is_substruct(Mol *p1, Mol *p2)
{
  assert(p1 && p2);
  RDKit::MatchVectType matchVect;
  return RDKit::SubstructMatch(*p1, *p2, matchVect) ? 1 : 0; 
}

int mol_is_superstruct(Mol *p1, Mol *p2)
{
  return mol_is_substruct(p2, p1);
}

int mol_cmp(Mol *p1, Mol *p2)
{
  assert(p1 && p2);
  
  int res = p1->getNumAtoms() - p2->getNumAtoms();
  if (res) {return (res > 0) ? 1 : -1;}

  res = p1->getNumBonds() - p2->getNumBonds();
  if (res) {return (res > 0) ? 1 : -1;}

  res = int(RDKit::Descriptors::calcAMW(*p1, false) -
	    RDKit::Descriptors::calcAMW(*p2, false) + .5);
  if (res) {return (res > 0) ? 1 : -1;}

  res = p1->getRingInfo()->numRings() - p2->getRingInfo()->numRings();
  if (res) {return (res > 0) ? 1 : -1;}

  // FIXME if not the same result is -1 also if the args are swapped
  return mol_is_substruct(p1, p2) ? 0 : -1; 
}

// Molecular descriptors /////////////////////////////////////////////////////

#define MOL_DESCRIPTOR(name, func, type)		\
  type mol_##name(Mol *pMol)				\
  {							\
    assert(pMol);					\
    return func(*pMol);					\
  }

MOL_DESCRIPTOR(mw, RDKit::Descriptors::calcAMW, double)
MOL_DESCRIPTOR(tpsa, RDKit::Descriptors::calcTPSA, double)
MOL_DESCRIPTOR(hba, RDKit::Descriptors::calcLipinskiHBA, int)
MOL_DESCRIPTOR(hbd, RDKit::Descriptors::calcLipinskiHBD, int)
MOL_DESCRIPTOR(num_rotatable_bnds, RDKit::Descriptors::calcNumRotatableBonds, 
	       int)
MOL_DESCRIPTOR(num_hetatms, RDKit::Descriptors::calcNumHeteroatoms, int)
MOL_DESCRIPTOR(num_rings, RDKit::Descriptors::calcNumRings, int)
MOL_DESCRIPTOR(chi0v,RDKit::Descriptors::calcChi0v,double)
MOL_DESCRIPTOR(chi1v,RDKit::Descriptors::calcChi1v,double)
MOL_DESCRIPTOR(chi2v,RDKit::Descriptors::calcChi2v,double)
MOL_DESCRIPTOR(chi3v,RDKit::Descriptors::calcChi3v,double)
MOL_DESCRIPTOR(chi4v,RDKit::Descriptors::calcChi4v,double)
MOL_DESCRIPTOR(chi0n,RDKit::Descriptors::calcChi0n,double)
MOL_DESCRIPTOR(chi1n,RDKit::Descriptors::calcChi1n,double)
MOL_DESCRIPTOR(chi2n,RDKit::Descriptors::calcChi2n,double)
MOL_DESCRIPTOR(chi3n,RDKit::Descriptors::calcChi3n,double)
MOL_DESCRIPTOR(chi4n,RDKit::Descriptors::calcChi4n,double)
MOL_DESCRIPTOR(kappa1,RDKit::Descriptors::calcKappa1,double)
MOL_DESCRIPTOR(kappa2,RDKit::Descriptors::calcKappa2,double)
MOL_DESCRIPTOR(kappa3,RDKit::Descriptors::calcKappa3,double)
  
double mol_logp(Mol *pMol) 
{
  assert(pMol);
  double logp, mr;
  RDKit::Descriptors::calcCrippenDescriptors(*pMol, logp, mr);
  return logp;
}

int mol_num_atms(Mol *pMol) 
{
  assert(pMol);
  return pMol->getNumAtoms(false);
}

int mol_num_hvyatms(Mol *pMol) 
{
  assert(pMol);
  return pMol->getNumAtoms(true);
}


// Bfp <-> Blob ///////////////////////////////////////////////////////

int bfp_to_blob(Bfp *pBfp, u8 **ppBlob, int *pLen)
{
  assert(pBfp);
  *ppBlob = 0;
  *pLen = 0;

  int rc = SQLITE_OK;

  *ppBlob = (u8 *)sqlite3_malloc(pBfp->size());
  if (*ppBlob) {
    memcpy(*ppBlob, pBfp->data(), pBfp->size());
    *pLen = pBfp->size();
  }
  else {
    rc = SQLITE_NOMEM;
  }

  return rc;
}

int blob_to_bfp(const u8 *pBlob, int len, Bfp **ppBfp)
{
  assert(pBlob);
  *ppBfp = new Bfp(reinterpret_cast<const char *>(pBlob), len);
  return SQLITE_OK;       
}

// Bfp <-> Blob ///////////////////////////////////////////////////////

double bfp_tanimoto(Bfp *pBfp1, Bfp *pBfp2)
{
  assert(pBfp1 && pBfp2);
  assert(pBfp1->size() == pBfp2->size());

  const u8 * afp = reinterpret_cast<const u8 *>(pBfp1->data());
  const u8 * bfp = reinterpret_cast<const u8 *>(pBfp2->data());
  int len = pBfp1->size();

  return bfp_op_tanimoto(len, const_cast<u8 *>(afp), const_cast<u8 *>(bfp));
}

double bfp_dice(Bfp *pBfp1, Bfp *pBfp2)
{
  assert(pBfp1 && pBfp2);
  assert(pBfp1->size() == pBfp2->size());

  const u8 * afp = reinterpret_cast<const u8 *>(pBfp1->data());
  const u8 * bfp = reinterpret_cast<const u8 *>(pBfp2->data());
  int len = pBfp1->size();

  return bfp_op_dice(len, const_cast<u8 *>(afp), const_cast<u8 *>(bfp));
}

int bfp_length(Bfp *pBfp)
{
  assert(pBfp);
  return 8*pBfp->size();
}

int bfp_weight(Bfp *pBfp)
{
  assert(pBfp);
  const u8 * fp = reinterpret_cast<const u8 *>(pBfp->data());
  int len = pBfp->size();

  return bfp_op_weight(len, const_cast<u8 *>(fp));
}


// Molecule -> Bfp /////////////////////////////////////////////////////

int mol_layered_bfp(Mol *pMol, Bfp **ppBfp)
{
  assert(pMol);

  int rc = SQLITE_OK;
  *ppBfp = 0;

  try {
    ExplicitBitVect *bv 
      = RDKit::LayeredFingerprintMol(*pMol, 0xFFFFFFFF, 1, 7, LAYERED_FP_SIZE);
    if (bv) {
      *ppBfp = new Bfp(BitVectToBinaryText(*bv));
      delete bv;
    }
    else {
      rc = SQLITE_ERROR;
    }
  } 
  catch (...) {
    // unknown exception
    rc = SQLITE_ERROR;
  }
        
  return rc;
}

int mol_rdkit_bfp(Mol *pMol, Bfp **ppBfp)
{
  assert(pMol);

  int rc = SQLITE_OK;
  *ppBfp = 0;

  try {
    ExplicitBitVect *bv 
      = RDKit::RDKFingerprintMol(*pMol, 1, 6, LAYERED_FP_SIZE, 2);
    if (bv) {
      *ppBfp = new Bfp(BitVectToBinaryText(*bv));
      delete bv;
    }
    else {
      rc = SQLITE_ERROR;
    }
  } 
  catch (...) {
    // unknown exception
    rc = SQLITE_ERROR;
  }
        
  return rc;
}

int mol_morgan_bfp(Mol *pMol, int radius, Bfp **ppBfp)
{
  assert(pMol);

  int rc = SQLITE_OK;
  *ppBfp = 0;

  try {
    std::vector<u32> invars(pMol->getNumAtoms());
    RDKit::MorganFingerprints::getConnectivityInvariants(*pMol, invars, true);
    ExplicitBitVect *bv 
      = RDKit::MorganFingerprints::getFingerprintAsBitVect(*pMol, radius,
							   MORGAN_FP_SIZE,
							   &invars);
    if (bv) {
      *ppBfp = new Bfp(BitVectToBinaryText(*bv));
      delete bv;
    }
    else {
      rc = SQLITE_ERROR;
    }
  } 
  catch (...) {
    // unknown exception
    rc = SQLITE_ERROR;
  }
        
  return rc;
}

int mol_feat_morgan_bfp(Mol *pMol, int radius, Bfp **ppBfp)
{
  assert(pMol);

  int rc = SQLITE_OK;
  *ppBfp = 0;

  try {
    std::vector<u32> invars(pMol->getNumAtoms());
    RDKit::MorganFingerprints::getFeatureInvariants(*pMol, invars);
    ExplicitBitVect *bv 
      = RDKit::MorganFingerprints::getFingerprintAsBitVect(*pMol, radius,
							   MORGAN_FP_SIZE,
							   &invars);
    if (bv) {
      *ppBfp = new Bfp(BitVectToBinaryText(*bv));
      delete bv;
    }
    else {
      rc = SQLITE_ERROR;
    }
  } 
  catch (...) {
    // unknown exception
    rc = SQLITE_ERROR;
  }
        
  return rc;
}

int mol_atom_pairs_bfp(Mol *pMol, Bfp **ppBfp)
{
  assert(pMol);

  int rc = SQLITE_OK;
  *ppBfp = 0;

  try {
    ExplicitBitVect *bv 
      = RDKit::AtomPairs::getHashedAtomPairFingerprintAsBitVect(*pMol,
								HASHED_PAIR_FP_SIZE);
    if (bv) {
      *ppBfp = new Bfp(BitVectToBinaryText(*bv));
      delete bv;
    }
    else {
      rc = SQLITE_ERROR;
    }
  } 
  catch (...) {
    // unknown exception
    rc = SQLITE_ERROR;
  }
        
  return rc;
}

int mol_topological_torsion_bfp(Mol *pMol, Bfp **ppBfp)
{
  assert(pMol);

  int rc = SQLITE_OK;
  *ppBfp = 0;

  try {
    ExplicitBitVect *bv 
      = RDKit::AtomPairs::getHashedTopologicalTorsionFingerprintAsBitVect(*pMol,
									  HASHED_TORSION_FP_SIZE);
    if (bv) {
      *ppBfp = new Bfp(BitVectToBinaryText(*bv));
      delete bv;
    }
    else {
      rc = SQLITE_ERROR;
    }
  } 
  catch (...) {
    // unknown exception
    rc = SQLITE_ERROR;
  }
        
  return rc;
}

int mol_maccs_bfp(Mol *pMol, Bfp **ppBfp)
{
  assert(pMol);

  int rc = SQLITE_OK;
  *ppBfp = 0;

  try {
    ExplicitBitVect *bv 
      = RDKit::MACCSFingerprints::getFingerprintAsBitVect(*pMol);
    if (bv) {
      *ppBfp = new Bfp(BitVectToBinaryText(*bv));
      delete bv;
    }
    else {
      rc = SQLITE_ERROR;
    }
  } 
  catch (...) {
    // unknown exception
    rc = SQLITE_ERROR;
  }
        
  return rc;
}

// Molecule -> signature /////////////////////////////////////////////////////

int mol_bfp_signature(Mol *pMol, Bfp **ppBfp)
{
  assert(pMol);

  int rc = SQLITE_OK;
  *ppBfp = 0;

  try {
    ExplicitBitVect *bv 
      = RDKit::PatternFingerprintMol(*pMol, SSS_FP_SIZE);
    if (bv) {
      *ppBfp = new Bfp(BitVectToBinaryText(*bv));
      delete bv;
    }
    else {
      rc = SQLITE_ERROR;
    }
  } 
  catch (...) {
    // unknown exception
    rc = SQLITE_ERROR;
  }
        
  return rc;
}

// RDKit version info ////////////////////////////////////////////////////////

const char * rdkit_version()
{
  return RDKit::rdkitVersion;
}

const char * boost_version()
{
  return RDKit::boostVersion;
}