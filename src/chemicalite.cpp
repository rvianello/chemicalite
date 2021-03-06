#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include "settings.hpp"
#include "mol_formats.hpp"
#include "mol_compare.hpp"
#include "mol_to_bfp.hpp"
#include "bfp_compare.hpp"
#include "versions.hpp"

#if 0
/*
** Create an rdtree index for a molecule column
*/
static void create_molecule_rdtree_f(sqlite3_context* ctx, 
				     int argc, sqlite3_value** argv)
{
  UNUSED(argc);
  assert(argc == 2);

  /* check arguments type */
  if (sqlite3_value_type(argv[0]) != SQLITE_TEXT ||
      sqlite3_value_type(argv[1]) != SQLITE_TEXT) {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    return;
  }

  sqlite3 *db = sqlite3_context_db_handle(ctx);
  const char *table = (const char *)sqlite3_value_text(argv[0]);
  const char *column = (const char *)sqlite3_value_text(argv[1]);

  int rc = SQLITE_OK;

  /* allocate the sql statements */

  char * create_index
    = sqlite3_mprintf("CREATE VIRTUAL TABLE 'str_idx_%q_%q' USING\n"
		      "rdtree(id, s bits(%d), OPT_FOR_SUBSET_QUERIES)",
		      table, column, DEFAULT_SSS_BFP_LENGTH);

  if (!create_index) {
    rc = SQLITE_NOMEM;
    goto stri_end;
  }

  char * create_insert_trigger
    = sqlite3_mprintf("CREATE TRIGGER 'strii_%q_%q' AFTER INSERT ON '%q'\n"
		      "FOR EACH ROW BEGIN\n"
		      "INSERT INTO 'str_idx_%q_%q'(id, s)\n"
		      "VALUES (NEW.ROWID, mol_bfp_signature(NEW.'%q'));\n"
		      "END;", 
		      table, column, table, table, column, column);

  if (!create_insert_trigger) {
    rc = SQLITE_NOMEM;
    goto stri_free_create;
  }

  char * create_update_trigger
    = sqlite3_mprintf("CREATE TRIGGER 'striu_%q_%q' AFTER UPDATE ON '%q'\n"
		      "FOR EACH ROW BEGIN\n"
		      "UPDATE 'str_idx_%q_%q' "
		      "SET s=mol_bfp_signature(NEW.'%q')\n"
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
		      "SELECT ROWID, mol_bfp_signature(%q) FROM '%q'",
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
#endif // #if 0

/*
** Register the chemicalite module with database handle db.
*/
#ifdef _WIN32
__declspec(dllexport)
#endif
extern "C" int sqlite3_chemicalite_init(sqlite3 *db, char ** /*pzErrMsg*/,
			   const sqlite3_api_routines *pApi)
{
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi)

  if (rc == SQLITE_OK) rc = chemicalite_init_versions(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_settings(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_mol_formats(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_mol_compare(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_mol_to_bfp(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_bfp_compare(db);

#if 0
  if (rc == SQLITE_OK) {
    rc = chemicalite_init_bitstring(db);
  }
  
  if (rc == SQLITE_OK) {
    rc = chemicalite_init_rdtree(db);
  }
  
  /* if (rc == SQLITE_OK) { */
  /*   rc = chemicalite_init_XYZ(db); */
  /* } */
#endif

#if 0
  CREATE_SQLITE_BINARY_FUNCTION(create_molecule_rdtree);
#endif

  return rc;
}
