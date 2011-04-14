#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include "rdkit_adapter.h"

static const int MAX_TXTMOL_LENGTH = 300;
static const int AS_SMILES = 0;
static const int AS_SMARTS = 1;

/*
** implementation for SMILES/SMARTS conversion to molecule object 
*/

static void convert_txt_to_molecule(sqlite3_context* ctx, 
				    int argc, sqlite3_value** argv,
				    int mode)
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
  int rc = txt_to_blob(sqlite3_value_text(argv[0]), mode, &pBlob, &sz);
  if (rc == SQLITE_OK) {
    sqlite3_result_blob(ctx, pBlob, sz, sqlite3_free);
  }
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

/*
** convert SMILES string into a molecule
*/
static void mol_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  convert_txt_to_molecule(ctx, argc, argv, AS_SMILES);
}

/*
** Convert SMARTS strings into query molecules
*/
static void qmol_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  convert_txt_to_molecule(ctx, argc, argv, AS_SMARTS);
}

/*
** fetch Mol from text or blob argument
*/

static int fetch_mol_arg(sqlite3_value* arg, Mol **ppMol)
{
  int rc = SQLITE_OK;

  /* Check that value is a blob */
  if (sqlite3_value_type(arg) == SQLITE_BLOB) {
    int sz = sqlite3_value_bytes(arg);
    rc = blob_to_mol((u8 *)sqlite3_value_blob(arg), sz, ppMol);
  }
  /* or a text string */
  else if (sqlite3_value_type(arg) == SQLITE3_TEXT) {
    rc = sqlite3_value_bytes(arg) <= MAX_TXTMOL_LENGTH ?
      txt_to_mol(sqlite3_value_text(arg), 0, ppMol) : SQLITE_TOOBIG;
  }
  else {
    rc = SQLITE_MISMATCH;
  }

  return rc;
}

/*
** convert molecule into a SMILES string
*/
static void mol_smiles_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  assert(argc == 1);
  int rc = SQLITE_OK;

  Mol *pMol = 0;
  char * smiles = 0;

  if ( ((rc = fetch_mol_arg(argv[0], &pMol)) != SQLITE_OK) ||
       ((rc = mol_to_txt(pMol, AS_SMILES, &smiles)) != SQLITE_OK) ) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_text(ctx, smiles, -1, sqlite3_free);
  }

  free_mol(pMol);
}

/*
** substructure match
*/

static void mol_is_substruct_f(sqlite3_context* ctx, 
			       int argc, sqlite3_value** argv)
{
  assert(argc == 2);
  int rc = SQLITE_OK;

  Mol *p1 = 0;
  Mol *p2 = 0;
  
  rc = fetch_mol_arg(argv[0], &p1);
  if (rc != SQLITE_OK) goto mol_is_substruct_f_end;

  rc = fetch_mol_arg(argv[1], &p2);
  if (rc != SQLITE_OK) goto mol_is_substruct_f_free_mol1;

  int result = mol_is_substruct(p1, p2) ? 1 : 0;

 mol_is_substruct_f_free_mol2: free_mol(p2);
 mol_is_substruct_f_free_mol1: free_mol(p1);
 mol_is_substruct_f_end:
  if (rc == SQLITE_OK) {
    sqlite3_result_int(ctx, result);
  }
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

/* same as function above but with args swapped */
static void mol_substruct_of_f(sqlite3_context* ctx, 
			       int argc, sqlite3_value** argv)
{
  assert(argc == 2);
  int rc = SQLITE_OK;

  Mol *p1 = 0;
  Mol *p2 = 0;
  
  rc = fetch_mol_arg(argv[0], &p1);
  if (rc != SQLITE_OK) goto mol_is_substruct_f_end;

  rc = fetch_mol_arg(argv[1], &p2);
  if (rc != SQLITE_OK) goto mol_is_substruct_f_free_mol1;

  int result = mol_is_substruct(p2, p1) ? 1 : 0;

 mol_is_substruct_f_free_mol2: free_mol(p2);
 mol_is_substruct_f_free_mol1: free_mol(p1);
 mol_is_substruct_f_end:
  if (rc == SQLITE_OK) {
    sqlite3_result_int(ctx, result);
  }
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

/*
** molecular descriptors
*/

static void compute_real_descriptor(sqlite3_context* ctx, 
				    int argc, sqlite3_value** argv,
				    double (*descriptor)(Mol *))
{
  assert(argc == 1);

  Mol *pMol = 0;
  int rc = fetch_mol_arg(argv[0], &pMol);

  if (rc == SQLITE_OK) {
    double mw = descriptor(pMol);
    free_mol(pMol);
    sqlite3_result_double(ctx, mw);
  }
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

static void mol_mw_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  compute_real_descriptor(ctx, argc, argv, mol_amw);
}

static void mol_logp_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  compute_real_descriptor(ctx, argc, argv, mol_logp);
}

static void mol_tpsa_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  compute_real_descriptor(ctx, argc, argv, mol_tpsa);
}

static void compute_int_descriptor(sqlite3_context* ctx, 
				   int argc, sqlite3_value** argv,
				   int (*descriptor)(Mol *))
{
  assert(argc == 1);

  Mol *pMol = 0;
  int rc = fetch_mol_arg(argv[0], &pMol);

  if (rc == SQLITE_OK) {
    double mw = descriptor(pMol);
    free_mol(pMol);
    sqlite3_result_int(ctx, mw);
  }
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

static void mol_hba_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  compute_int_descriptor(ctx, argc, argv, mol_hba);
}

static void mol_hbd_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  compute_int_descriptor(ctx, argc, argv, mol_hbd);
}

static void mol_num_atms_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  compute_int_descriptor(ctx, argc, argv, mol_num_atms);
}

static void mol_num_hvyatms_f(sqlite3_context* ctx, 
			      int argc, sqlite3_value** argv)
{
  compute_int_descriptor(ctx, argc, argv, mol_num_hvyatms);
}

static void mol_num_rotatable_bnds_f(sqlite3_context* ctx, 
				     int argc, sqlite3_value** argv)
{
  compute_int_descriptor(ctx, argc, argv, mol_num_rotatable_bnds);
}

static void mol_num_hetatms_f(sqlite3_context* ctx, 
			      int argc, sqlite3_value** argv)
{
  compute_int_descriptor(ctx, argc, argv, mol_num_hetatms);
}

static void mol_num_rings_f(sqlite3_context* ctx, 
			    int argc, sqlite3_value** argv)
{
  compute_int_descriptor(ctx, argc, argv, mol_num_rings);
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

  int rc = fetch_mol_arg(argv[0], &pMol);

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
    rc = sqlite3_create_function(db, "qmol",
				 1, SQLITE_UTF8, 0, qmol_f, 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_smiles",
				 1, SQLITE_UTF8, 0, mol_smiles_f, 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_is_substruct",
				 2, SQLITE_UTF8, 0, mol_is_substruct_f, 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_substruct_of",
				 2, SQLITE_UTF8, 0, mol_substruct_of_f, 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_mw",
				 1, SQLITE_UTF8, 0, mol_mw_f, 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_logp",
				 1, SQLITE_UTF8, 0, mol_logp_f, 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_tpsa",
				 1, SQLITE_UTF8, 0, mol_tpsa_f, 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_hba",
				 1, SQLITE_UTF8, 0, mol_hba_f, 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_hbd",
				 1, SQLITE_UTF8, 0, mol_hbd_f, 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_num_atms",
				 1, SQLITE_UTF8, 0, mol_num_atms_f, 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_num_hvyatms",
				 1, SQLITE_UTF8, 0, mol_num_hvyatms_f, 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_num_rotatable_bnds",
				 1, SQLITE_UTF8, 0, mol_num_rotatable_bnds_f, 
				 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_num_hetatms",
				 1, SQLITE_UTF8, 0, mol_num_hetatms_f, 
				 0, 0);
  }
  
  if (rc == SQLITE_OK) {
    rc = sqlite3_create_function(db, "mol_num_rings",
				 1, SQLITE_UTF8, 0, mol_num_rings_f, 
				 0, 0);
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
