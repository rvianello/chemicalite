#include <memory>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/MolStandardize/MolStandardize.h>

#include "utils.hpp"
#include "mol_standardize.hpp"
#include "mol.hpp"
#include "logging.hpp"

static void free_params_auxdata(void * aux)
{
  delete (RDKit::MolStandardize::CleanupParameters *) aux;
}

typedef RDKit::RWMol * (*MolStandardizerFunc)(const RDKit::RWMol *mol, const RDKit::MolStandardize::CleanupParameters &params);

template <MolStandardizerFunc F>
void mol_standardize(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  int rc = SQLITE_OK;
  sqlite3_value *arg = nullptr;
  const RDKit::MolStandardize::CleanupParameters *params
    = &RDKit::MolStandardize::defaultCleanupParameters;

  // the input molecule
  arg = argv[0];
  std::unique_ptr<RDKit::RWMol> mol_in(arg_to_rwmol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  if (argc > 1) {
    void *aux = sqlite3_get_auxdata(ctx, 1);
    if (aux) {
      params = (RDKit::MolStandardize::CleanupParameters *) aux; 
    }
    else {
      arg = argv[1];
      int value_type = sqlite3_value_type(arg);
      if (value_type != SQLITE_TEXT) {
        sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
        chemicalite_log(SQLITE_MISMATCH, "update_params arg must be of type text");
        return;
      }
      std::string json = (const char *)sqlite3_value_text(arg);
      std::unique_ptr<RDKit::MolStandardize::CleanupParameters>
        updated_params(new RDKit::MolStandardize::CleanupParameters);
      try {
        RDKit::MolStandardize::updateCleanupParamsFromJSON(*updated_params, json);
      }
      catch (...) {
        sqlite3_result_error_code(ctx, SQLITE_ERROR);
        chemicalite_log(SQLITE_ERROR, ("could not parse update_params arg: '" + json + "'").c_str());
        return;        
      }
      params = updated_params.release();
      sqlite3_set_auxdata(ctx, 1, (void *) params, free_params_auxdata);
    }
  }

  std::unique_ptr<RDKit::RWMol> mol_out(F(mol_in.get(), *params));

  Blob blob = mol_to_blob(*mol_out, &rc);
  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
  }
}

void (*mol_cleanup)(sqlite3_context*, int, sqlite3_value**) = strict<mol_standardize<RDKit::MolStandardize::cleanup>>;
void (*mol_normalize)(sqlite3_context*, int, sqlite3_value**) = strict<mol_standardize<RDKit::MolStandardize::normalize>>;
void (*mol_reionize)(sqlite3_context*, int, sqlite3_value**) = strict<mol_standardize<RDKit::MolStandardize::reionize>>;
void (*mol_remove_fragments)(sqlite3_context*, int, sqlite3_value**) = strict<mol_standardize<RDKit::MolStandardize::removeFragments>>;
void (*mol_canonical_tautomer)(sqlite3_context*, int, sqlite3_value**) = strict<mol_standardize<RDKit::MolStandardize::canonicalTautomer>>;


typedef RDKit::RWMol * (*MolParentFunc)(const RDKit::RWMol &mol, const RDKit::MolStandardize::CleanupParameters &params, bool skip_standardize);

template <MolParentFunc F>
void mol_parent(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  int rc = SQLITE_OK;
  sqlite3_value *arg = nullptr;
  const RDKit::MolStandardize::CleanupParameters *params
    = &RDKit::MolStandardize::defaultCleanupParameters;
  bool skip_standardize = false;

  // the input molecule
  arg = argv[0];
  std::unique_ptr<RDKit::RWMol> mol_in(arg_to_rwmol(arg, &rc));
  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  if (argc > 1) {
    void *aux = sqlite3_get_auxdata(ctx, 1);
    if (aux) {
      params = (RDKit::MolStandardize::CleanupParameters *) aux; 
    }
    else {
      arg = argv[1];
      int value_type = sqlite3_value_type(arg);
      if (value_type != SQLITE_TEXT) {
        sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
        chemicalite_log(SQLITE_MISMATCH, "update_params arg must be of type text");
        return;
      }
      std::string json = (const char *)sqlite3_value_text(arg);
      std::unique_ptr<RDKit::MolStandardize::CleanupParameters>
        updated_params(new RDKit::MolStandardize::CleanupParameters);
      try {
        RDKit::MolStandardize::updateCleanupParamsFromJSON(*updated_params, json);
      }
      catch (...) {
        sqlite3_result_error_code(ctx, SQLITE_ERROR);
        chemicalite_log(SQLITE_ERROR, ("could not parse update_params arg: '" + json + "'").c_str());
        return;        
      }
      params = updated_params.release();
      sqlite3_set_auxdata(ctx, 1, (void *) params, free_params_auxdata);
    }
  }

  if (argc > 2) {
    arg = argv[2];
    int value_type = sqlite3_value_type(arg);
    if (value_type != SQLITE_INTEGER) {
      sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
      chemicalite_log(SQLITE_MISMATCH, "skip_standardize arg must be of type INTEGER (bool)");
      return;
    }
    skip_standardize = sqlite3_value_int(arg);
  }

  std::unique_ptr<RDKit::RWMol> mol_out(F(*mol_in, *params, skip_standardize));

  Blob blob = mol_to_blob(*mol_out, &rc);
  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
  }
}

void (*mol_tautomer_parent)(sqlite3_context*, int, sqlite3_value**) = strict<mol_parent<RDKit::MolStandardize::tautomerParent>>;
void (*mol_fragment_parent)(sqlite3_context*, int, sqlite3_value**) = strict<mol_parent<RDKit::MolStandardize::fragmentParent>>;
void (*mol_stereo_parent)(sqlite3_context*, int, sqlite3_value**) = strict<mol_parent<RDKit::MolStandardize::stereoParent>>;
void (*mol_isotope_parent)(sqlite3_context*, int, sqlite3_value**) = strict<mol_parent<RDKit::MolStandardize::isotopeParent>>;
void (*mol_charge_parent)(sqlite3_context*, int, sqlite3_value**) = strict<mol_parent<RDKit::MolStandardize::chargeParent>>;
void (*mol_super_parent)(sqlite3_context*, int, sqlite3_value**) = strict<mol_parent<RDKit::MolStandardize::superParent>>;


int chemicalite_init_mol_standardize(sqlite3 *db)
{
  int rc = SQLITE_OK;

  for (int argc=1; argc<3; ++argc) {
    if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_cleanup", argc, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, mol_cleanup, 0, 0);
    if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_normalize", argc, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, mol_normalize, 0, 0);
    if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_reionize", argc, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, mol_reionize, 0, 0);
    if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_remove_fragments", argc, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, mol_remove_fragments, 0, 0);
    if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_canonical_tautomer", argc, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, mol_canonical_tautomer, 0, 0);
  }

  for (int argc=1; argc<4; ++argc) {
    if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_tautomer_parent", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, mol_tautomer_parent, 0, 0);
    if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_fragment_parent", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, mol_fragment_parent, 0, 0);
    if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_stereo_parent", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, mol_stereo_parent, 0, 0);
    if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_isotope_parent", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, mol_isotope_parent, 0, 0);
    if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_charge_parent", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, mol_charge_parent, 0, 0);
    if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_super_parent", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, mol_super_parent, 0, 0);
  }

  return rc;
}
