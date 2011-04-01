#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include "rdkit_adapter.h"

static const int MAX_TXTMOL_LENGTH = 300;

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
  if (len > MAX_TXTMOL_LENGTH) {
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
    rc = blob_to_mol((u8 *)sqlite3_value_blob(argv[0]), sz, &pMol);
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
** Molecule bitstring signature
*/

static void mol_signature_f(sqlite3_context* ctx, 
			    int argc, sqlite3_value** argv)
{
  assert(argc == 1);

  Mol *pMol = 0;
  u8 *signature = 0;
  int len = 0;
  int rc = SQLITE_ERROR; /* defensively pessimistic */

  /* Check that value is a blob */
  if (sqlite3_value_type(argv[0]) == SQLITE_BLOB) {
    int sz = sqlite3_value_bytes(argv[0]);
    rc = blob_to_mol((u8 *)sqlite3_value_blob(argv[0]), sz, &pMol);
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
    rc = mol_signature(pMol, &signature, &len);
    free_mol(pMol);
  }
  
  if (rc == SQLITE_OK) {
    assert(len == MOL_SIGNATURE_SIZE);
    sqlite3_result_blob(ctx, signature, len, sqlite3_free);
  }
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

/*
** create a structural index
*/

static void mol_structural_index_f(sqlite3_context* ctx, 
				   int argc, sqlite3_value** argv)
{
  assert(argc == 2);

  /* check arguments type */
  if (sqlite3_value_type(argv[0]) != SQLITE_TEXT ||
      sqlite3_value_type(argv[1]) != SQLITE_TEXT) {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    return;
  }

  sqlite3 *db = sqlite3_context_db_handle(ctx);
  const char *table = sqlite3_value_text(argv[0]);
  const char *column = sqlite3_value_text(argv[1]);

  int rc = SQLITE_OK;

  /* allocate the sql statements */

  char * create_index
    = sqlite3_mprintf("CREATE VIRTUAL TABLE 'str_idx_%q_%q' USING\n"
		      "signtree(id, s bytes(%d))",
		      table, column, MOL_SIGNATURE_SIZE);

  if (!create_index) {
    rc = SQLITE_NOMEM;
    goto stri_end;
  }

  char * create_insert_trigger
    = sqlite3_mprintf("CREATE TRIGGER 'strii_%q_%q' AFTER INSERT ON '%q'\n"
		      "FOR EACH ROW BEGIN\n"
		      "INSERT INTO 'str_idx_%q_%q'(id, s)\n"
		      "VALUES (NEW.ROWID, mol_signature(NEW.'%q'));\n"
		      "END;", 
		      table, column, table, table, column, column);

  if (!create_insert_trigger) {
    rc = SQLITE_NOMEM;
    goto stri_free_create;
  }

  char * create_update_trigger
    = sqlite3_mprintf("CREATE TRIGGER 'striu_%q_%q' AFTER UPDATE ON '%q'\n"
		      "FOR EACH ROW BEGIN\n"
		      "UPDATE 'str_idx_%q_%q' SET s=mol_signature(NEW.'%q')\n"
		      "WHERE id=NEW.ROWID;\n"
		      "END;", 
		      table, column, table, table, column, column);

  if (!create_update_trigger) {
    rc = SQLITE_NOMEM;
    goto stri_free_insert;
  }

  char * create_delete_trigger
    = sqlite3_mprintf("CREATE TRIGGER 'strid_%q_%q' AFTER DELETE ON '%q'\n"
		      "FOR EACH ROW BEGIN\n"
		      "DELETE FROM 'str_idx_%q_%q' WHERE id=OLD.ROWID;\n"
		      "END;",
		      table, column, table, table, column);

  if (!create_delete_trigger) {
    rc = SQLITE_NOMEM;
    goto stri_free_update;
  }

  char * load_index
    = sqlite3_mprintf("INSERT INTO 'str_idx_%q_%q' (id, s) "
		      "SELECT ROWID, mol_signature(%q) FROM '%q'",
		      table, column, column, table);

  if (!load_index) {
    rc = SQLITE_NOMEM;
    goto stri_free_delete;
  }

  /* create the index */
  rc = sqlite3_exec(db, create_index, NULL, NULL, NULL);
  if (rc != SQLITE_OK) goto stri_free_load;

  /* INSERT trigger */
  rc = sqlite3_exec(db, create_insert_trigger, NULL, NULL, NULL);
  if (rc != SQLITE_OK) goto stri_free_load;
 
  /* UPDATE trigger */
  rc = sqlite3_exec(db, create_update_trigger, NULL, NULL, NULL);
  if (rc != SQLITE_OK) goto stri_free_load;

  /* DELETE trigger */
  rc = sqlite3_exec(db, create_delete_trigger, NULL, NULL, NULL);
  if (rc != SQLITE_OK) goto stri_free_load;

  /* load the index with data */
  rc = sqlite3_exec(db, load_index, NULL, NULL, NULL);

  /* free the allocated statements */
 stri_free_load:  
  sqlite3_free(load_index);

 stri_free_delete:  
  sqlite3_free(create_delete_trigger);

 stri_free_update:  
  sqlite3_free(create_update_trigger);

 stri_free_insert:  
  sqlite3_free(create_insert_trigger);

 stri_free_create:  
  sqlite3_free(create_index);

 stri_end:  
  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_int(ctx, 1);
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
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_signature",
				 1, SQLITE_UTF8, 0, mol_signature_f, 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_structural_index",
				 2, SQLITE_UTF8, 0, 
				 mol_structural_index_f, 0, 0);
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
