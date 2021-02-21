#ifndef CHEMICALITE_MOLECULE_INCLUDED
#define CHEMICALITE_MOLECULE_INCLUDED
#include <memory>
#include <string>

namespace RDKit
{
  class ROMol;
  class RWMol;
} // namespace RDKit

std::string mol_to_binary(const RDKit::ROMol &);

std::unique_ptr<RDKit::ROMol> binary_to_romol(const std::string &);
std::unique_ptr<RDKit::RWMol> binary_to_rwmol(const std::string &);

std::unique_ptr<RDKit::ROMol> arg_to_romol(sqlite3_value *, sqlite3_context *, int *);
std::unique_ptr<RDKit::RWMol> arg_to_rwmol(sqlite3_value *, sqlite3_context *, int *);

#endif
