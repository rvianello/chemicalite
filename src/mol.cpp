#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/MolPickler.h>

#include "utils.hpp"
#include "mol.hpp"
#include "logging.hpp"

std::string mol_to_binary_mol(const RDKit::ROMol & mol)
{
  std::string buf;
  try {
    RDKit::MolPickler::pickleMol(
      mol, buf,
      RDKit::PicklerOps::AllProps | RDKit::PicklerOps::CoordsAsDouble);
  }
  catch (...) {
    chemicalite_log(SQLITE_ERROR, "Could not serialize mol to binary");
  }
  return buf;
}

std::string binary_mol_to_blob(const std::string & bmol)
{
  // return unmodified for now
  return bmol;
}

std::string mol_to_blob(const RDKit::ROMol & mol)
{
  std::string bmol = mol_to_binary_mol(mol);
  return bmol.size() ? binary_mol_to_blob(bmol) : bmol;
}

std::string blob_to_binary_mol(const std::string &buf)
{
  // return unmodified for now
  return buf;
}

template <typename MolT>
MolT * binary_mol_to_mol(const std::string & bmol)
{
  try {
    return new MolT(bmol);
  }
  catch (...) {
    chemicalite_log(SQLITE_ERROR, "Could not deserialize mol from binary");
  }
  return nullptr;
}

RDKit::ROMol * binary_mol_to_romol(const std::string & bmol)
{
  return binary_mol_to_mol<RDKit::ROMol>(bmol);
}

RDKit::RWMol * binary_mol_to_rwmol(const std::string & bmol)
{
  return binary_mol_to_mol<RDKit::RWMol>(bmol);
}

template <typename MolT>
MolT * blob_to_mol(const std::string &buf)
{
  std::string bmol = blob_to_binary_mol(buf);
  return binary_mol_to_mol<MolT>(bmol);
}

RDKit::ROMol * blob_to_romol(const std::string &buf)
{
  return blob_to_mol<RDKit::ROMol>(buf);
}

RDKit::RWMol * blob_to_rwmol(const std::string &buf)
{
  return blob_to_mol<RDKit::RWMol>(buf);
}

std::string arg_to_binary_mol(sqlite3_value *arg, int *rc)
{
  int value_type = sqlite3_value_type(arg);

  *rc = SQLITE_OK;

  if (value_type != SQLITE_BLOB) {
    *rc = SQLITE_MISMATCH;
    chemicalite_log(SQLITE_MISMATCH, "input arg must be of type blob or NULL");
  }
  else {
    std::string blob((const char *)sqlite3_value_blob(arg), sqlite3_value_bytes(arg));
    return blob_to_binary_mol(blob);
  }

  return "";
}

template <typename MolT>
MolT * arg_to_mol(sqlite3_value *arg, int *rc)
{
  int value_type = sqlite3_value_type(arg);

  *rc = SQLITE_OK;

  if (value_type != SQLITE_BLOB) {
    *rc = SQLITE_MISMATCH;
    chemicalite_log(SQLITE_MISMATCH, "input arg must be of type blob or NULL");
  }
  else {
    std::string blob((const char *)sqlite3_value_blob(arg), sqlite3_value_bytes(arg));
    return blob_to_mol<MolT>(blob);
  }

  return nullptr;
}

RDKit::ROMol * arg_to_romol(sqlite3_value *arg, int *rc)
{
  return arg_to_mol<RDKit::ROMol>(arg, rc);
}

RDKit::RWMol * arg_to_rwmol(sqlite3_value *arg, int *rc)
{
  return arg_to_mol<RDKit::RWMol>(arg, rc);
}
