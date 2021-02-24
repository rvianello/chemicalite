#ifndef CHEMICALITE_MOLECULE_INCLUDED
#define CHEMICALITE_MOLECULE_INCLUDED
#include <string>

namespace RDKit
{
  class ROMol;
  class RWMol;
} // namespace RDKit

std::string mol_to_binary(const RDKit::ROMol &);

RDKit::ROMol * binary_to_romol(const std::string &);
RDKit::RWMol * binary_to_rwmol(const std::string &);

RDKit::ROMol * arg_to_romol(sqlite3_value *, sqlite3_context *, int *);
RDKit::RWMol * arg_to_rwmol(sqlite3_value *, sqlite3_context *, int *);

#endif
