#include <cstring>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/MolPickler.h>

#include "utils.hpp"
#include "mol.hpp"
#include "logging.hpp"

static constexpr const uint32_t MOL_MAGIC = 0x4D4F4C00;

std::string mol_to_binary_mol(const RDKit::ROMol & mol, int * rc)
{
  std::string buf;
  try {
    RDKit::MolPickler::pickleMol(mol, buf, RDKit::PicklerOps::MolProps);
  }
  catch (...) {
    *rc = SQLITE_ERROR;
    chemicalite_log(*rc, "Could not serialize mol to binary");
  }
  return buf;
}

Blob binary_mol_to_blob(const std::string & bmol, int *)
{
  Blob blob(sizeof(uint32_t) + bmol.size());
  uint8_t * p = blob.data();
  p += write_uint32(p, MOL_MAGIC);
  memcpy(p, bmol.data(), bmol.size());
  return blob;
}

Blob mol_to_blob(const RDKit::ROMol & mol, int * rc)
{
  std::string bmol = mol_to_binary_mol(mol, rc);
  if (*rc == SQLITE_OK) {
    return binary_mol_to_blob(bmol, rc);
  }
  return Blob();
}

std::string blob_to_binary_mol(const Blob &blob, int *rc)
{
  if (blob.size() <= sizeof(uint32_t)) {
    *rc = SQLITE_MISMATCH;
    return "";
  }
  const uint8_t * p = blob.data();
  uint32_t magic = read_uint32(p);
  if (magic != MOL_MAGIC) {
    *rc = SQLITE_MISMATCH;
    chemicalite_log(*rc, "mismatching blob header found");
    return "";
  }
  p += sizeof(uint32_t);
  return std::string(reinterpret_cast<const char *>(p), blob.size()-sizeof(uint32_t));
}

template <typename MolT>
MolT * binary_mol_to_mol(const std::string & bmol, int * rc)
{
  try {
    std::unique_ptr<MolT> mol(new MolT());
    RDKit::MolPickler::molFromPickle(bmol, *mol);
    return mol.release();
  }
  catch (...) {
    *rc = SQLITE_ERROR;
    chemicalite_log(*rc, "Could not deserialize mol from binary");
  }
  return nullptr;
}

RDKit::ROMol * binary_mol_to_romol(const std::string & bmol, int * rc)
{
  return binary_mol_to_mol<RDKit::ROMol>(bmol, rc);
}

RDKit::RWMol * binary_mol_to_rwmol(const std::string & bmol, int * rc)
{
  return binary_mol_to_mol<RDKit::RWMol>(bmol, rc);
}

template <typename MolT>
MolT * blob_to_mol(const Blob &blob, int * rc)
{
  std::string bmol = blob_to_binary_mol(blob, rc);
  if (*rc == SQLITE_OK) {
    return binary_mol_to_mol<MolT>(bmol, rc);
  }
  return nullptr;
}

RDKit::ROMol * blob_to_romol(const Blob &blob, int * rc)
{
  return blob_to_mol<RDKit::ROMol>(blob, rc);
}

RDKit::RWMol * blob_to_rwmol(const Blob &blob, int * rc)
{
  return blob_to_mol<RDKit::RWMol>(blob, rc);
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
    uint8_t * data = (uint8_t *) sqlite3_value_blob(arg);
    Blob blob(data, data + sqlite3_value_bytes(arg));
    return blob_to_binary_mol(blob, rc);
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
    uint8_t * data = (uint8_t *) sqlite3_value_blob(arg);
    Blob blob(data, data + sqlite3_value_bytes(arg));
    return blob_to_mol<MolT>(blob, rc);
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

void free_romol_auxdata(void * aux)
{
  delete (RDKit::ROMol *) aux;
}

