#ifndef CHEMICALITE_MOLECULE_INCLUDED
#define CHEMICALITE_MOLECULE_INCLUDED
#include <string>

namespace RDKit
{
  class ROMol;
  class RWMol;
} // namespace RDKit

std::string mol_to_binary_mol(const RDKit::ROMol &, int *);
Blob binary_mol_to_blob(const std::string &, int *);
Blob mol_to_blob(const RDKit::ROMol &, int *);

std::string blob_to_binary_mol(const Blob &, int *);
RDKit::ROMol * binary_mol_to_romol(const std::string &, int *);
RDKit::RWMol * binary_mol_to_rwmol(const std::string &, int *);
RDKit::ROMol * blob_to_romol(const Blob &, int *);
RDKit::RWMol * blob_to_rwmol(const Blob &, int *);

std::string arg_to_binary_mol(sqlite3_value *, int *);
RDKit::ROMol * arg_to_romol(sqlite3_value *, int *);
RDKit::RWMol * arg_to_rwmol(sqlite3_value *, int *);

#endif
