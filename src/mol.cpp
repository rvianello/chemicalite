#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/MolPickler.h>

#include "mol.hpp"
//#include "logging.hpp"
//#include "utils.hpp"

std::string mol_to_binary(const RDKit::ROMol *mol, int *rc)
{
  std::string buf;
  try {
    RDKit::MolPickler::pickleMol(
      mol, buf,
      RDKit::PicklerOps::AllProps | RDKit::PicklerOps::CoordsAsDouble);
    *rc = SQLITE_OK;
  }
  catch (...) {
    *rc = SQLITE_ERROR;
  }
  return buf;
}

template <typename MolT>
MolT * binary_to_mol(const std::string &buf, int *rc)
{
  MolT * mol = nullptr;
  try {
    mol = new MolT(buf);
    *rc = SQLITE_OK;
  }
  catch (...) {
    *rc = SQLITE_ERROR;
  }
  return mol;
}

RDKit::ROMol * binary_to_romol(const std::string &buf, int *rc)
{
  return binary_to_mol<RDKit::ROMol>(buf, rc);
}

RDKit::RWMol * binary_to_rwmol(const std::string &buf, int *rc)
{
  return binary_to_mol<RDKit::RWMol>(buf, rc);
}
