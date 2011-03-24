#include "rdkit_adapter.h"

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <cassert>
#include <string>

#include <GraphMol/RDKitBase.h>
#include <GraphMol/Descriptors/MolDescriptors.h>
#include <GraphMol/SmilesParse/SmilesParse.h>

struct CMol : RDKit::ROMol {};

void free_cmolecule(CMol *pCMol)
{
  delete static_cast<RDKit::ROMol *>(pCMol);
}

int txt_to_cmol(const char * txt, bool as_smarts, CMol **ppCMol)
{
  assert(txt);
  int rc = SQLITE_OK;

  *ppCMol = 0;

  try {
    std::string data(txt);
    RDKit::ROMol *pROMol = as_smarts ?
      RDKit::SmartsToMol(data) : RDKit::SmilesToMol(data);
      *ppCMol = static_cast<CMol *>(pROMol);
  } catch (...) {
    // problem generating molecule from smiles
    rc = SQLITE_ERROR;
  }
  if (!*ppCMol) {
    // parse error
    rc = SQLITE_ERROR;
  }

  return rc;
}

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

