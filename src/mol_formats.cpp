#include <cassert>
#include <string>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/SmilesParse/SmilesParse.h>
#include <GraphMol/FileParsers/FileParsers.h>
#include <GraphMol/SmilesParse/SmilesWrite.h>

#include "utils.hpp"
#include "mol.hpp"
#include "mol_formats.hpp"
#include "logging.hpp"

static void mol_to_smiles(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  sqlite3_value *arg = argv[0];

  int rc = SQLITE_OK;
  std::unique_ptr<RDKit::ROMol> mol = arg_to_romol(arg, ctx, &rc);

  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    std::string smiles = RDKit::MolToSmiles(*mol);
    sqlite3_result_text(ctx, smiles.c_str(), -1, SQLITE_TRANSIENT);
  }
}

static void mol_from_smiles(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  sqlite3_value *arg = argv[0];
  int value_type = sqlite3_value_type(arg);

  /* not a string */
  if (value_type != SQLITE3_TEXT) {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    chemicalite_log(SQLITE_MISMATCH, "input arg must be of type text or NULL");
    return;
  }

  /* build the molecule binary repr from a text string */
  std::string smiles = (const char *)sqlite3_value_text(arg);
  std::unique_ptr<RDKit::ROMol> mol;

  try {
    mol.reset(RDKit::SmilesToMol(smiles));
  }
  catch (...) {
    chemicalite_log(
      SQLITE_ERROR,
      "Conversion from '%s' to mol triggered an exception.",
      smiles.c_str()
      );
    sqlite3_result_null(ctx);
    return;
  }

  if (mol) {
    std::string buf = mol_to_binary(*mol);
    if (buf.empty()) {
      sqlite3_result_error_code(ctx, SQLITE_ERROR);
    }
    else {
      sqlite3_result_blob(ctx, buf.c_str(), buf.size(), SQLITE_TRANSIENT);
    }
  }
  else {
    chemicalite_log(
      SQLITE_WARNING,
      "Could not convert '%s' into mol.",
      smiles.c_str()
      );
    sqlite3_result_null(ctx);
  }
}

static void mol_to_molblock(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  sqlite3_value *arg = argv[0];

  int rc = SQLITE_OK;
  std::unique_ptr<RDKit::ROMol> mol = arg_to_romol(arg, ctx, &rc);

  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    std::string molblock = RDKit::MolToMolBlock(*mol);
    sqlite3_result_text(ctx, molblock.c_str(), -1, SQLITE_TRANSIENT);
  }
}

static void mol_from_molblock(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  sqlite3_value *arg = argv[0];
  int value_type = sqlite3_value_type(arg);

  /* not a string */
  if (value_type != SQLITE3_TEXT) {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    chemicalite_log(SQLITE_MISMATCH, "input arg must be of type text or NULL");
    return;
  }

  /* build the molecule binary repr from a text string */
  std::string molblock = (const char *)sqlite3_value_text(arg);
  std::unique_ptr<RDKit::ROMol> mol;

  try {
    mol.reset(RDKit::MolBlockToMol(molblock));
  }
  catch (...) {
    chemicalite_log(
      SQLITE_ERROR,
      "Conversion from molblock to mol triggered an exception."
      );
    sqlite3_result_null(ctx);
    return;
  }

  if (mol) {
    std::string buf = mol_to_binary(*mol);
    if (buf.empty()) {
      sqlite3_result_error_code(ctx, SQLITE_ERROR);
    }
    else {
      sqlite3_result_blob(ctx, buf.c_str(), buf.size(), SQLITE_TRANSIENT);
    }
  }
  else {
    chemicalite_log(
      SQLITE_WARNING,
      "Could not convert molblock into mol."
      );
    sqlite3_result_null(ctx);
  }
}

int chemicalite_init_mol_formats(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_from_smiles", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_from_smiles>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_from_molblock", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_from_molblock>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_to_smiles", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_smiles>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_to_molblock", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_to_molblock>, 0, 0);

  return rc;
}
