#ifndef CHEMICALITE_MOLECULE_INCLUDED
#define CHEMICALITE_MOLECULE_INCLUDED
#include <string>

namespace RDKit
{
  class ROMol;
  class RWMol;
} // namespace RDKit

std::string mol_to_binary(const RDKit::ROMol *, int *);
RDKit::ROMol * binary_to_romol(const std::string &, int*);
RDKit::RWMol * binary_to_rwmol(const std::string &, int*);

#endif
