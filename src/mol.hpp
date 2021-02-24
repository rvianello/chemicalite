#ifndef CHEMICALITE_MOLECULE_INCLUDED
#define CHEMICALITE_MOLECULE_INCLUDED
#include <string>

namespace RDKit
{
  class ROMol;
  class RWMol;
} // namespace RDKit

std::string mol_to_binary_mol(const RDKit::ROMol &);
std::string binary_mol_to_blob(const std::string &);
std::string mol_to_blob(const RDKit::ROMol &);

std::string blob_to_binary_mol(const std::string &);
RDKit::ROMol * binary_mol_to_romol(const std::string &);
RDKit::RWMol * binary_mol_to_rwmol(const std::string &);
RDKit::ROMol * blob_to_romol(const std::string &);
RDKit::RWMol * blob_to_rwmol(const std::string &);

std::string arg_to_binary_mol(sqlite3_value *, sqlite3_context *, int *);
RDKit::ROMol * arg_to_romol(sqlite3_value *, sqlite3_context *, int *);
RDKit::RWMol * arg_to_rwmol(sqlite3_value *, sqlite3_context *, int *);

#endif
