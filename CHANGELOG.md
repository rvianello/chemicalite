# Changelog

## [2024.02.1] - 2024-02-08

### Changed

- Updated the RDKit required version in the CMake config to 2023.09.1

## [2022.04.1] - 2022-04-05

### Fixed

- #8 - stop using std::result_of (esp. if not needed).

## [2022.01.2] - 2022-01-30

### Added

- Mol standardization functions.

## [2022.01.1] - 2022-01-21

### Added

- `sdf_writer` aggregate function.
- `smi_reader` and `smi_writer` functions.

### Changed

- `sdf_reader` can be also used as a table-valued function
- Mol serialization extended again to AllProps (reverting a change in 2021.07.1)
 
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
