#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/MolPickler.h>

#include "utils.hpp"
#include "mol.hpp"
#include "logging.hpp"

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

template <typename MolT>
MolT * arg_to_mol(sqlite3_value *arg, sqlite3_context * ctx, int *rc)
{
  int value_type = sqlite3_value_type(arg);

  *rc = SQLITE_OK;
  MolT *mol = nullptr;

  /* NULL on NULL */
  if (value_type == SQLITE_NULL) {
    sqlite3_result_null(ctx);
  }
  /* not a binary blob */
  else if (value_type != SQLITE_BLOB) {
    *rc = SQLITE_MISMATCH;
    chemicalite_log(SQLITE_MISMATCH, "input arg must be of type blob or NULL");
  }
  else {
    std::string blob((const char *)sqlite3_value_blob(arg), sqlite3_value_bytes(arg));
    mol = binary_to_mol<MolT>(blob, rc);
  }

  return mol;
}

RDKit::ROMol * arg_to_romol(sqlite3_value *arg, sqlite3_context * ctx, int *rc)
{
  return arg_to_mol<RDKit::ROMol>(arg, ctx, rc);
}

RDKit::RWMol * arg_to_rwmol(sqlite3_value *arg, sqlite3_context * ctx, int *rc)
{
  return arg_to_mol<RDKit::RWMol>(arg, ctx, rc);
}
