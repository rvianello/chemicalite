# Changelog

## [2021.05.2] - 2021-05-10

### Changed

- Compiler settings for using the optimized popcnt instructions (-mpopcnt replaces -march=native)

## [2021.05.1] - 2021-05-08

### Changed

- Most of the code was migrated from C to C++ and substantially rewritten. The API is in general similar to 
  the earlier version, but some implicit assumptions and default behaviors have been removed (e.g. implicit
  promotions of function args from SMILES string to mol are no longer supported). Please refer to the docs
  for further details (the available documentation was updated, it will be hopefully expanded soon).

### Added

- This Changelog file.

## [2020.12.5] - 2020-12-23
