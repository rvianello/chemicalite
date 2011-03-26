#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include "rdkit_adapter.h"

static const int MAX_TXT_LENGTH = 300;

/*
** convert SMILES string into a blob-molecule
*/
static void mol_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  assert(argc == 1);

  /* Check that value is actually a string */
  if (sqlite3_value_type(argv[0]) != SQLITE3_TEXT) {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    return;
  }

  /* verify the string is not too big for a SMILES */
  int len = sqlite3_value_bytes(argv[0]);
  if (len > MAX_TXT_LENGTH) {
    sqlite3_result_error_toobig(ctx);
    return;
  }
  
  u8 *pBlob = 0;
  int sz = 0;
  int rc = txt_to_blob(sqlite3_value_text(argv[0]), 0, &pBlob, &sz);
  if (rc == SQLITE_OK) {
    sqlite3_result_blob(ctx, pBlob, sz, sqlite3_free);
  }
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

/*
** molecular descriptors
*/

static void mol_mw_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  assert(argc == 1);

  Mol *pMol = 0;
  int rc = SQLITE_ERROR; /* defensively pessimistic */

  /* Check that value is a blob */
  if (sqlite3_value_type(argv[0]) == SQLITE_BLOB) {
    int sz = sqlite3_value_bytes(argv[0]);
    rc = blob_to_mol((u8 *)sqlite3_value_text(argv[0]), sz, &pMol);
  }
  /* or a text string */
  else if (sqlite3_value_type(argv[0]) == SQLITE3_TEXT) {
    rc = txt_to_mol(sqlite3_value_text(argv[0]), 0, &pMol);
  }
  else {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    return;
  }

  if (rc == SQLITE_OK) {
    double mw = mol_amw(pMol);
    free_mol(pMol);
    sqlite3_result_double(ctx, mw);
  }
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

/*
** Register the chemicalite module with database handle db.
*/
int sqlite3_chemicalite_init(sqlite3 *db)
{
  int rc = SQLITE_OK;
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol",
				 1, SQLITE_UTF8, 0, mol_f, 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_mw",
				 1, SQLITE_UTF8, 0, mol_mw_f, 0, 0);
  }
  

  /*
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_module_v2(db, [...]);
  }
  */

  return rc;
}

int sqlite3_extension_init(sqlite3 *db, char **pzErrMsg,
			   const sqlite3_api_routines *pApi)
{
  SQLITE_EXTENSION_INIT2(pApi)
  return sqlite3_chemicalite_init(db);
}
