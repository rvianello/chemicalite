# Changelog

## [2021.07.1] - 2021-07-12

### Changed

- Mol serialization is now limited to MolProps.
- The strategy for indexing binary fingerprints into the `rdtree` virtual table was substantially reimplemented.

## [2021.06.1] - 2021-06-11

### Added

- `mol_find_mcs` aggregate function.
- Substructure-based mol transformations + Murcko decomposition.
- `sdf_reader` virtual table.
- Bindings to RDKit's Periodic Table.

## [2021.05.2] - 2021-05-10

### Changed

- Compiler settings for using the optimized popcnt instructions (-mpopcnt replaces -march=native).

## [2021.05.1] - 2021-05-08

### Changed

- Most of the code was migrated from C to C++ and substantially rewritten. The API is in general similar to 
  the earlier version, but some implicit assumptions and default behaviors have been removed (e.g. implicit
  promotions of function args from SMILES string to mol are no longer supported). Please refer to the docs
  for further details (the available documentation was updated, it will be hopefully expanded soon).

### Added

- This Changelog file.

## [2020.12.5] - 2020-12-23
