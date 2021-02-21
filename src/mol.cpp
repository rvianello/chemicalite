#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/MolPickler.h>

#include "utils.hpp"
#include "mol.hpp"
#include "logging.hpp"

std::string mol_to_binary(const RDKit::ROMol & mol)
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

template <typename MolT>
std::unique_ptr<MolT> binary_to_mol(const std::string &buf)
{
  try {
    return std::unique_ptr<MolT>(new MolT(buf));
  }
  catch (...) {
    chemicalite_log(SQLITE_ERROR, "Could not deserialize mol from binary");
  }
  return std::unique_ptr<MolT>();
}

std::unique_ptr<RDKit::ROMol> binary_to_romol(const std::string &buf)
{
  return binary_to_mol<RDKit::ROMol>(buf);
}

std::unique_ptr<RDKit::RWMol> binary_to_rwmol(const std::string &buf)
{
  return binary_to_mol<RDKit::RWMol>(buf);
}

template <typename MolT>
std::unique_ptr<MolT> arg_to_mol(sqlite3_value *arg, sqlite3_context * /*ctx*/, int *rc)
{
  int value_type = sqlite3_value_type(arg);

  *rc = SQLITE_OK;

  if (value_type != SQLITE_BLOB) {
    *rc = SQLITE_MISMATCH;
    chemicalite_log(SQLITE_MISMATCH, "input arg must be of type blob or NULL");
  }
  else {
    std::string blob((const char *)sqlite3_value_blob(arg), sqlite3_value_bytes(arg));
    return binary_to_mol<MolT>(blob);
  }

  return std::unique_ptr<MolT>();
}

std::unique_ptr<RDKit::ROMol> arg_to_romol(sqlite3_value *arg, sqlite3_context * ctx, int *rc)
{
  return arg_to_mol<RDKit::ROMol>(arg, ctx, rc);
}

std::unique_ptr<RDKit::RWMol> arg_to_rwmol(sqlite3_value *arg, sqlite3_context * ctx, int *rc)
{
  return arg_to_mol<RDKit::RWMol>(arg, ctx, rc);
}
