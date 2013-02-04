#include "rdkit_adapter.h"

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <cassert>
#include <cstring>
#include <string>

#include <GraphMol/RDKitBase.h>
#include <GraphMol/MolPickler.h>
#include <GraphMol/Descriptors/MolDescriptors.h>
#include <GraphMol/SmilesParse/SmilesParse.h>
#include <GraphMol/SmilesParse/SmartsWrite.h>
#include <GraphMol/SmilesParse/SmilesWrite.h>
#include <GraphMol/Substruct/SubstructMatch.h>
#include <GraphMol/Fingerprints/Fingerprints.h>
#include <DataStructs/ExplicitBitVect.h>
#include <DataStructs/BitOps.h>

struct Mol : RDKit::ROMol {
  // Mol() : RDKit::ROMol() {}
  // Mol(const Mol & other) : RDKit::ROMol(other) {}
  // Mol(const RDKit::ROMol & other) : RDKit::ROMol(other) {}
  Mol(const std::string & pickle) : RDKit::ROMol(pickle) {}
};

void free_mol(Mol *pMol)
{
  delete static_cast<RDKit::ROMol *>(pMol);
}

struct BitString : ExplicitBitVect {
  BitString(const char * d, const unsigned int n) : ExplicitBitVect(d, n) {}
};

void free_bitstring(BitString *pBits)
{
  delete static_cast<ExplicitBitVect *>(pBits);
}

namespace {
  const unsigned int SSS_FP_SIZE         = 8*MOL_SIGNATURE_SIZE;
  const unsigned int LAYERED_FP_SIZE     = 1024;
  const unsigned int MORGAN_FP_SIZE      = 1024;
  const unsigned int HASHED_PAIR_FP_SIZE = 2048;
}

// SMILES/SMARTS <-> Molecule ////////////////////////////////////////////////

int txt_to_mol(const char * txt, int as_smarts, Mol **ppMol)
{
  assert(txt);
  int rc = SQLITE_OK;

  *ppMol = 0;

  try {
    std::string data(txt);
    RDKit::ROMol *pROMol = as_smarts ?
      RDKit::SmartsToMol(data) : RDKit::SmilesToMol(data);
      *ppMol = static_cast<Mol *>(pROMol);
  } 
  catch (...) {
    // problem generating molecule from smiles
    rc = SQLITE_ERROR;
  }
  if (!*ppMol) {
    // parse error
    rc = SQLITE_ERROR;
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

int blob_to_mol(u8 *pBlob, int len, Mol **ppMol)
{
  assert(pBlob);
  *ppMol = 0;

  int rc = SQLITE_OK;

  std::string blob;
  try {
    blob.assign((const char *)pBlob, len);
    *ppMol = new Mol(blob);
  } 
  catch (...) {
    // problem generating molecule from blob data
    rc = SQLITE_ERROR;
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

// Molecule -> signature /////////////////////////////////////////////////////

int mol_signature(Mol *pMol, u8 **ppSign, int *pLen)
{
  assert(pMol);

  *ppSign = 0;
  *pLen = 0;

  int rc = SQLITE_OK;
  BitString *pBits = 0;

  try {
    ExplicitBitVect *bv 
      = RDKit::LayeredFingerprintMol(*pMol, RDKit::substructLayers, 1, 6, 
				     SSS_FP_SIZE);
    if (bv) {
      rc = bitstring_to_blob(static_cast<BitString *>(bv), ppSign, pLen);
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

// Molecules comparison //////////////////////////////////////////////////////

int mol_is_substruct(Mol *p1, Mol *p2)
{
  assert(p1 && p2);
  RDKit::MatchVectType matchVect;
  return RDKit::SubstructMatch(*p1, *p2, matchVect); 
}

int mol_cmp(Mol *p1, Mol *p2)
{
  if(!p1 && !p2) { 
    return 0;
  }
  else if (!p1) {
    return -1;
  }
  else if (!p2) {
    return 1;
  }
  
  int res = p1->getNumAtoms() - p2->getNumAtoms();
  if (res) {return (res > 0) ? 1 : -1;}

  res = p1->getNumBonds() - p2->getNumBonds();
  if (res) {return (res > 0) ? 1 : -1;}

  res = int(RDKit::Descriptors::calcAMW(*p1, false) -
	    RDKit::Descriptors::calcAMW(*p2, false) + .5);
  if (res) {return (res > 0) ? 1 : -1;}

  res = p1->getRingInfo()->numRings() - p2->getRingInfo()->numRings();
  if (res) {return (res > 0) ? 1 : -1;}

  return mol_is_substruct(p1, p2) ? 0 : -1;
}

// Molecular descriptors /////////////////////////////////////////////////////

double mol_mw(Mol *pMol) 
{
  assert(pMol);
  return RDKit::Descriptors::calcAMW(*pMol, false);
}

double mol_logp(Mol *pMol) 
{
  assert(pMol);
  double logp, mr;
  RDKit::Descriptors::calcCrippenDescriptors(*pMol, logp, mr);
  return logp;
}

double mol_tpsa(Mol *pMol) 
{
  assert(pMol);
  return RDKit::Descriptors::calcTPSA(*pMol);
}
  
int mol_hba(Mol *pMol) 
{
  assert(pMol);
  return RDKit::Descriptors::calcLipinskiHBA(*pMol);
}

int mol_hbd(Mol *pMol) 
{
  assert(pMol);
  return RDKit::Descriptors::calcLipinskiHBD(*pMol);
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

int mol_num_rotatable_bnds(Mol *pMol) 
{
  assert(pMol);
  RDKit::Descriptors::calcNumRotatableBonds(*pMol);
}

int mol_num_hetatms(Mol *pMol) 
{
  assert(pMol);
  return RDKit::Descriptors::calcNumHeteroatoms(*pMol);
}

int mol_num_rings(Mol *pMol) 
{
  assert(pMol);
  return RDKit::Descriptors::calcNumRings(*pMol);
}

// BitString <-> Blob ///////////////////////////////////////////////////////

int bitstring_to_blob(BitString *pBits, u8 **ppBlob, int *pLen)
{
  assert(pBits);
  *ppBlob = 0;
  *pLen = 0;

  int rc = SQLITE_OK;

  int num_bits = pBits->getNumBits();
  int num_bytes = num_bits/8;
  if (num_bits % 8) ++num_bytes;

  *ppBlob = (u8 *)sqlite3_malloc(num_bytes);

  u8 *s = *ppBlob;

  if (!s) {
    rc = SQLITE_NOMEM;
  }
  else {
    memset(s, 0, num_bytes);
    *pLen = num_bytes;
    for (int i = 0; i < num_bits; ++i) {
      if (!pBits->getBit(i)) { continue; }
      s[ i/8 ]  |= 1 << (i % 8);
    }
  }

  return rc;
}

int blob_to_bitstring(u8 *pBlob, int len, BitString **ppBits)
{
  assert(pBlob);
  int rc = SQLITE_OK;
  *ppBits = 0;
        
  try {
    if (!( *ppBits = new BitString((const char *)pBlob, len) )) {
      rc = SQLITE_NOMEM;
    }
  } catch (...) {
    // Unknown exception
    rc = SQLITE_ERROR;
  }

  return rc;       
}

// BitString <-> Blob ///////////////////////////////////////////////////////

int bitstring_tanimoto(BitString *pBits1, BitString *pBits2, double *pSim)
{
  int rc = SQLITE_OK;
  *pSim = 0.0;

  // Nsame / (Na + Nb - Nsame)
        
  try {
    *pSim = TanimotoSimilarity(*static_cast<ExplicitBitVect *>(pBits1), 
			       *static_cast<ExplicitBitVect *>(pBits2));
  } 
  catch (ValueErrorException& e) {
    // TODO investigate possible causes for this exc
    rc = SQLITE_ERROR;
  } 
  catch (...) {
    // unknown exception
    rc = SQLITE_ERROR;
  }

  return rc;
}

int bitstring_dice(BitString *pBits1, BitString *pBits2, double *pSim)
{
  int rc = SQLITE_OK;
  *pSim = 0.0;

  // 2 * Nsame / (Na + Nb)
        
  try {
    *pSim = DiceSimilarity(*static_cast<ExplicitBitVect *>(pBits1), 
			   *static_cast<ExplicitBitVect *>(pBits2));
  } 
  catch (ValueErrorException& e) {
    // TODO investigate possible causes for this exc
    rc = SQLITE_ERROR;
  } 
  catch (...) {
    // unknown exception
    rc = SQLITE_ERROR;
  }

  return rc;
}
