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

struct CMol : RDKit::ROMol {
  CMol() : RDKit::ROMol() {}
  CMol(const CMol & other) : RDKit::ROMol(other) {}
  CMol(const RDKit::ROMol & other) : RDKit::ROMol(other) {}
  CMol(const std::string & pickle) : RDKit::ROMol(pickle) {}
};

void free_cmol(CMol *pCMol)
{
  delete static_cast<RDKit::ROMol *>(pCMol);
}

// SMILES/SMARTS <-> Molecule ////////////////////////////////////////////////

int txt_to_cmol(const char * txt, int as_smarts, CMol **ppCMol)
{
  assert(txt);
  int rc = SQLITE_OK;

  *ppCMol = 0;

  try {
    std::string data(txt);
    RDKit::ROMol *pROMol = as_smarts ?
      RDKit::SmartsToMol(data) : RDKit::SmilesToMol(data);
      *ppCMol = static_cast<CMol *>(pROMol);
  } 
  catch (...) {
    // problem generating molecule from smiles
    rc = SQLITE_ERROR;
  }
  if (!*ppCMol) {
    // parse error
    rc = SQLITE_ERROR;
  }

  return rc;
}

int cmol_to_txt(CMol *pCMol, int as_smarts, char **pTxt)
{
  assert(pCMol);
  *pTxt = 0;
  int rc = SQLITE_OK;

  std::string txt;
  try {
    txt.assign( as_smarts ? 
		RDKit::MolToSmarts(*pCMol, false) : 
		RDKit::MolToSmiles(*pCMol, true) );
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

int blob_to_cmol(u8 *pBlob, int len, CMol **ppCMol)
{
  assert(pBlob);
  *ppCMol = 0;

  int rc = SQLITE_OK;

  std::string blob;
  try {
    blob.assign((const char *)pBlob, len);
    *ppCMol = new CMol(blob);
  } 
  catch (...) {
    // problem generating molecule from blob data
    rc = SQLITE_ERROR;
  }
  
  if (!(*ppCMol)) {
    // blob data could not be parsed
    rc = SQLITE_ERROR;
  }

  return rc;
}

int cmol_to_blob(CMol *pCMol, u8 **ppBlob, int *pLen)
{
  assert(pCMol);

  *ppBlob = 0;
  *pLen = 0;

  int rc = SQLITE_OK;
  std::string blob;
  try {
    RDKit::MolPickler::pickleMol(*pCMol, blob);
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
  CMol * pCMol = 0;
  int rc = txt_to_cmol(txt, as_smarts, &pCMol);
  if (rc == SQLITE_OK) {
    rc = cmol_to_blob(pCMol, pBlob, pLen);
    free_cmol(pCMol);
  }
  return rc;
}

int blob_to_txt(u8 *blob, int len, int as_smarts, char **pTxt)
{
  CMol * pCMol = 0;
  int rc = blob_to_cmol(blob, len, &pCMol);
  if (rc == SQLITE_OK) {
    rc = cmol_to_txt(pCMol, as_smarts, pTxt);
    free_cmol(pCMol);
  }
  return rc;
}

// Molecular descriptors /////////////////////////////////////////////////////

double cmol_amw(CMol *pCMol) 
{
  assert(pCMol);
  return RDKit::Descriptors::calcAMW(*pCMol, false);
}

double cmol_logp(CMol *pCMol) 
{
  assert(pCMol);
  double logp, mr;
  RDKit::Descriptors::calcCrippenDescriptors(*pCMol, logp, mr);
  return logp;
}

double cmol_tpsa(CMol *pCMol) 
{
  assert(pCMol);
  return RDKit::Descriptors::calcTPSA(*pCMol);
}
  
int cmol_hba(CMol *pCMol) 
{
  assert(pCMol);
  return RDKit::Descriptors::calcLipinskiHBA(*pCMol);
}

int cmol_hbd(CMol *pCMol) 
{
  assert(pCMol);
  return RDKit::Descriptors::calcLipinskiHBD(*pCMol);
}

int cmol_num_atms(CMol *pCMol) 
{
  assert(pCMol);
  return pCMol->getNumAtoms(false);
}

int cmol_num_hvyatms(CMol *pCMol) 
{
  assert(pCMol);
  return pCMol->getNumAtoms(true);
}

int cmol_num_rotatable_bnds(CMol *pCMol) 
{
  assert(pCMol);
  RDKit::Descriptors::calcNumRotatableBonds(*pCMol);
}

int cmol_num_hetatms(CMol *pCMol) 
{
  assert(pCMol);
  return RDKit::Descriptors::calcNumHeteroatoms(*pCMol);
}

int cmol_num_rings(CMol *pCMol) 
{
  assert(pCMol);
  return RDKit::Descriptors::calcNumRings(*pCMol);
}

