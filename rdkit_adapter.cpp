#include "rdkit_adapter.h"

#include <cassert>

#include <GraphMol/RDKitBase.h>
#include <GraphMol/Descriptors/MolDescriptors.h>

struct CMol : RDKit::ROMol {};

void free_cmolecule(CMol *cmol)
{
  delete cmol;
}

double cmol_amw(CMol *cmol) 
{
  assert(cmol);
  return RDKit::Descriptors::calcAMW(*cmol, false);
}

double cmol_logp(CMol *cmol) 
{
  assert(cmol);
  double logp, mr;
  RDKit::Descriptors::calcCrippenDescriptors(*cmol, logp, mr);
  return logp;
}

double cmol_tpsa(CMol *cmol) 
{
  assert(cmol);
  return RDKit::Descriptors::calcTPSA(*cmol);
}
  
int cmol_hba(CMol *cmol) 
{
  assert(cmol);
  return RDKit::Descriptors::calcLipinskiHBA(*cmol);
}

int cmol_hbd(CMol *cmol) 
{
  assert(cmol);
  return RDKit::Descriptors::calcLipinskiHBD(*cmol);
}

int cmol_num_atms(CMol *cmol) 
{
  assert(cmol);
  return cmol->getNumAtoms(false);
}

int cmol_num_hvyatms(CMol *cmol) 
{
  assert(cmol);
  return cmol->getNumAtoms(true);
}

int cmol_num_rotatable_bnds(CMol *cmol) 
{
  assert(cmol);
  RDKit::Descriptors::calcNumRotatableBonds(*cmol);
}

int cmol_num_hetatms(CMol *cmol) 
{
  assert(cmol);
  return RDKit::Descriptors::calcNumHeteroatoms(*cmol);
}

int cmol_num_rings(CMol *cmol) 
{
  assert(cmol);
  return RDKit::Descriptors::calcNumRings(*cmol);
}

