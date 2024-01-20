#include <cassert>
#include <cstring>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"
#include "rdtree.hpp"
#include "rdtree_vtab.hpp"
#include "rdtree_constraint_subset.hpp"
#include "rdtree_constraint_tanimoto.hpp"
#include "bfp.hpp"

/* 
** RDtree virtual table module xCreate method.
*/
static int rdtreeCreate(sqlite3 *db, void *paux,
			int argc, const char *const*argv,
			sqlite3_vtab **pvtab,
			char **pzErr)
{
  return RDtreeVtab::create(db, paux, argc, argv, pvtab, pzErr);
}

/* 
** RDtree virtual table module xConnect method.
*/
static int rdtreeConnect(sqlite3 *db, void *paux,
			 int argc, const char *const*argv,
			 sqlite3_vtab **pvtab,
			 char **pzErr)
{
  return RDtreeVtab::connect(db, paux, argc, argv, pvtab, pzErr);
}

/* 
** RDtree virtual table module xBestIndex method.
*/
static int rdtreeBestIndex(sqlite3_vtab *vtab, sqlite3_index_info *idxinfo)
{
  RDtreeVtab *rdtree = (RDtreeVtab *)vtab;
  return rdtree->bestindex(idxinfo);
}

/* 
** RDtree virtual table module xDisconnect method.
*/
static int rdtreeDisconnect(sqlite3_vtab *vtab)
{
  RDtreeVtab *rdtree = (RDtreeVtab *)vtab;
  return rdtree->disconnect();
}

/* 
** RDtree virtual table module xDestroy method.
*/
static int rdtreeDestroy(sqlite3_vtab *vtab)
{
  RDtreeVtab *rdtree = (RDtreeVtab *)vtab;
  return rdtree->destroy();
}

/* 
** RDtree virtual table module xOpen method.
*/
static int rdtreeOpen(sqlite3_vtab *vtab, sqlite3_vtab_cursor **cursor)
{
  RDtreeVtab *rdtree = (RDtreeVtab *)vtab;
  return rdtree->open(cursor);
}

/* 
** RDtree virtual table module xClose method.
*/
static int rdtreeClose(sqlite3_vtab_cursor *cursor)
{
  RDtreeVtab *rdtree = (RDtreeVtab *)cursor->pVtab;
  return rdtree->close(cursor);
}

/* 
** RDtree virtual table module xFilter method.
*/
static int rdtreeFilter(sqlite3_vtab_cursor *cursor, 
			int idxnum, const char *idxstr,
			int argc, sqlite3_value **argv)
{
  RDtreeVtab *rdtree = (RDtreeVtab *)cursor->pVtab;
  return rdtree->filter(cursor, idxnum, idxstr, argc, argv);
}

/* 
** RDtree virtual table module xNext method.
*/
static int rdtreeNext(sqlite3_vtab_cursor *cursor)
{
  RDtreeVtab *rdtree = (RDtreeVtab *)cursor->pVtab;
  return rdtree->next(cursor);
}

/* 
** RDtree virtual table module xEof method.
*/
static int rdtreeEof(sqlite3_vtab_cursor *cursor)
{
  RDtreeVtab *rdtree = (RDtreeVtab *)cursor->pVtab;
  return rdtree->eof(cursor);
}

/* 
** RDtree virtual table module xColumn method.
*/
static int rdtreeColumn(sqlite3_vtab_cursor *cursor, sqlite3_context *ctx, int col)
{
  RDtreeVtab *rdtree = (RDtreeVtab *)cursor->pVtab;
  return rdtree->column(cursor, ctx, col);
}

/* 
** RDtree virtual table module xRowid method.
*/
static int rdtreeRowid(sqlite3_vtab_cursor *cursor, sqlite_int64 *pRowid)
{
  RDtreeVtab *rdtree = (RDtreeVtab *)cursor->pVtab;
  return rdtree->rowid(cursor, pRowid);
}

/* 
** RDtree virtual table module xUpdate method.
*/
static int rdtreeUpdate(
  sqlite3_vtab *vtab, int argc, sqlite3_value **argv, sqlite_int64 *rowid)
{
  RDtreeVtab *rdtree = (RDtreeVtab *)vtab;
  return rdtree->update(argc, argv, rowid);
}

/* 
** RDtree virtual table module xRename method.
*/
static int rdtreeRename(sqlite3_vtab *vtab, const char *newname)
{
  RDtreeVtab *rdtree = (RDtreeVtab *)vtab;
  return rdtree->rename(newname);
}


static sqlite3_module rdtreeModule = {
#if SQLITE_VERSION_NUMBER >= 3044000
  4,                           /* iVersion */
#else
  3,                           /* iVersion */
#endif
  rdtreeCreate,                /* xCreate - create a table */
  rdtreeConnect,               /* xConnect - connect to an existing table */
  rdtreeBestIndex,             /* xBestIndex - Determine search strategy */
  rdtreeDisconnect,            /* xDisconnect - Disconnect from a table */
  rdtreeDestroy,               /* xDestroy - Drop a table */
  rdtreeOpen,                  /* xOpen - open a cursor */
  rdtreeClose,                 /* xClose - close a cursor */
  rdtreeFilter,                /* xFilter - configure scan constraints */
  rdtreeNext,                  /* xNext - advance a cursor */
  rdtreeEof,                   /* xEof */
  rdtreeColumn,                /* xColumn - read data */
  rdtreeRowid,                 /* xRowid - read data */
  rdtreeUpdate,                /* xUpdate - write data */
  0,                           /* xBegin - begin transaction */
  0,                           /* xSync - sync transaction */
  0,                           /* xCommit - commit transaction */
  0,                           /* xRollback - rollback transaction */
  0,                           /* xFindFunction - function overloading */
  rdtreeRename,                /* xRename - rename the table */
  0,                           /* xSavepoint */
  0,                           /* xRelease */
  0,                           /* xRollbackTo */
  0                            /* xShadowName */
#if SQLITE_VERSION_NUMBER >= 3044000
  ,
  0                            /* xIntegrity */
#endif
};

/*
** Connect an rdtree index to a mol column
*/
static void rdtree_link_index(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  assert(argc == 5 || argc == 6);

  /* check arguments type */
  if (sqlite3_value_type(argv[0]) != SQLITE_TEXT || // table
      sqlite3_value_type(argv[1]) != SQLITE_TEXT || // column
      sqlite3_value_type(argv[2]) != SQLITE_TEXT || // rdtree
      sqlite3_value_type(argv[3]) != SQLITE_TEXT) { // bfp_constructor
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    return;
  }

  for (int argn = 4; argn < argc; ++argn) {
    // the additional args are int arguments passed to the bfp constructor.
    // in most cases it's the bfp length, for morgan bfps the radius is also
    // needed.
    if (sqlite3_value_type(argv[argn]) != SQLITE_INTEGER) {
      sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
      return;
    }
  }

  sqlite3 *db = sqlite3_context_db_handle(ctx);
  const char *table = (const char *)sqlite3_value_text(argv[0]);
  const char *column = (const char *)sqlite3_value_text(argv[1]);
  const char *rdtree = (const char *)sqlite3_value_text(argv[2]);
  const char *bfp_constructor = (const char *)sqlite3_value_text(argv[3]);

  std::string bfp_args = "";
  for (int argn = 4; argn < argc; ++argn) {
    bfp_args += ", ";
    bfp_args += std::to_string(sqlite3_value_int(argv[argn]));
  }

  char * pragma_table_info_rdtree;
  sqlite3_stmt *table_info_rdtree_stmt;
  std::string rdtree_id;
  std::string rdtree_bfp;

  char * create_insert_trigger;
  char * create_update_trigger;
  char * create_delete_trigger;

  int rc = SQLITE_OK;

  pragma_table_info_rdtree
    = sqlite3_mprintf("PRAGMA table_info('%q')", rdtree);

  if (!pragma_table_info_rdtree) {
      rc = SQLITE_NOMEM;
      goto ti_rdtree_end;
  }

  rc = sqlite3_prepare_v2(db, pragma_table_info_rdtree, -1, &table_info_rdtree_stmt, 0);
  if (rc != SQLITE_OK) {
    goto stmt_rdtree_end;
  }

  rc = sqlite3_step(table_info_rdtree_stmt);
  if (rc != SQLITE_ROW) {
    rc = SQLITE_ERROR;
    goto rdtree_info_end;
  }

  rdtree_id = (const char *) sqlite3_column_text(table_info_rdtree_stmt, 1);

  rc = sqlite3_step(table_info_rdtree_stmt);
  if (rc != SQLITE_ROW) {
    rc = SQLITE_ERROR;
    goto rdtree_info_end;
  }

  rdtree_bfp = (const char *) sqlite3_column_text(table_info_rdtree_stmt, 1);

  create_insert_trigger
    = sqlite3_mprintf("CREATE TRIGGER '%q_insert_%q_%q' AFTER INSERT ON '%q'\n"
		      "FOR EACH ROW BEGIN\n"
		      "INSERT INTO '%q'('%q', '%q')\n"
		      "VALUES (NEW.ROWID, \"%w\"(NEW.'%q'%s));\n"
		      "END;", 
		      rdtree, table, column, table,
          rdtree, rdtree_id.c_str(), rdtree_bfp.c_str(),
          bfp_constructor, column, bfp_args.c_str());

  if (!create_insert_trigger) {
    rc = SQLITE_NOMEM;
    goto stri_end;
  }

  create_update_trigger
    = sqlite3_mprintf("CREATE TRIGGER '%q_update_%q_%q' AFTER UPDATE ON '%q'\n"
		      "FOR EACH ROW BEGIN\n"
		      "UPDATE '%q' "
		      "SET '%q'=\"%w\"(NEW.'%q'%s)\n"
		      "WHERE '%q'=NEW.ROWID;\n"
		      "END;", 
		      rdtree, table, column, table,
          rdtree,
          rdtree_bfp.c_str(), bfp_constructor, column, bfp_args.c_str(),
          rdtree_id.c_str());

  if (!create_update_trigger) {
    rc = SQLITE_NOMEM;
    goto stri_free_insert;
  }

  create_delete_trigger
    = sqlite3_mprintf("CREATE TRIGGER '%q_delete_%q_%q' AFTER DELETE ON '%q'\n"
		      "FOR EACH ROW BEGIN\n"
		      "DELETE FROM '%q' WHERE '%q'=OLD.ROWID;\n"
		      "END;",
		      rdtree, table, column, table,
          rdtree, rdtree_id.c_str());

  if (!create_delete_trigger) {
    rc = SQLITE_NOMEM;
    goto stri_free_update;
  }

  /* INSERT trigger */
  rc = sqlite3_exec(db, create_insert_trigger, NULL, NULL, NULL);
  if (rc != SQLITE_OK) goto stri_free_delete;
 
  /* UPDATE trigger */
  rc = sqlite3_exec(db, create_update_trigger, NULL, NULL, NULL);
  if (rc != SQLITE_OK) goto stri_free_delete;

  /* DELETE trigger */
  rc = sqlite3_exec(db, create_delete_trigger, NULL, NULL, NULL);

  /* free the allocated statements */
 stri_free_delete:  
  sqlite3_free(create_delete_trigger);

 stri_free_update:  
  sqlite3_free(create_update_trigger);

 stri_free_insert:  
  sqlite3_free(create_insert_trigger);

 stri_end:
 rdtree_info_end:
  sqlite3_finalize(table_info_rdtree_stmt);

 stmt_rdtree_end:
  sqlite3_free(pragma_table_info_rdtree);

 ti_rdtree_end:
  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_int(ctx, 1);
  }
}

/*
** Disconnect an rdtree index from a mol column
*/
static void rdtree_unlink_index(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  /* check arguments type */
  if (sqlite3_value_type(argv[0]) != SQLITE_TEXT || // table
      sqlite3_value_type(argv[1]) != SQLITE_TEXT || // column
      sqlite3_value_type(argv[2]) != SQLITE_TEXT) { // rdtree
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    return;
  }

  sqlite3 *db = sqlite3_context_db_handle(ctx);
  const char *table = (const char *)sqlite3_value_text(argv[0]);
  const char *column = (const char *)sqlite3_value_text(argv[1]);
  const char *rdtree = (const char *)sqlite3_value_text(argv[2]);

  char * drop_insert_trigger;
  char * drop_update_trigger;
  char * drop_delete_trigger;

  int rc = SQLITE_OK;

  drop_insert_trigger
    = sqlite3_mprintf("DROP TRIGGER IF EXISTS '%q_insert_%q_%q'", 
		      rdtree, table, column);

  if (!drop_insert_trigger) {
    rc = SQLITE_NOMEM;
    goto stri_end;
  }

  drop_update_trigger
    = sqlite3_mprintf("DROP TRIGGER IF EXISTS '%q_update_%q_%q'", 
		      rdtree, table, column);

  if (!drop_update_trigger) {
    rc = SQLITE_NOMEM;
    goto stri_free_insert;
  }

  drop_delete_trigger
    = sqlite3_mprintf("DROP TRIGGER IF EXISTS '%q_delete_%q_%q'",
		      rdtree, table, column);

  if (!drop_delete_trigger) {
    rc = SQLITE_NOMEM;
    goto stri_free_update;
  }

  /* INSERT trigger */
  rc = sqlite3_exec(db, drop_insert_trigger, NULL, NULL, NULL);
  if (rc != SQLITE_OK) goto stri_free_delete;
 
  /* UPDATE trigger */
  rc = sqlite3_exec(db, drop_update_trigger, NULL, NULL, NULL);
  if (rc != SQLITE_OK) goto stri_free_delete;

  /* DELETE trigger */
  rc = sqlite3_exec(db, drop_delete_trigger, NULL, NULL, NULL);

  /* free the allocated statements */
 stri_free_delete:  
  sqlite3_free(drop_delete_trigger);

 stri_free_update:  
  sqlite3_free(drop_update_trigger);

 stri_free_insert:  
  sqlite3_free(drop_insert_trigger);

 stri_end:
  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_int(ctx, 1);
  }
}

/*
** A factory function for a substructure search match object
*/
static void rdtree_subset(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  int rc = SQLITE_OK;
  sqlite3_value *arg = argv[0];
  std::string bfp = arg_to_bfp(arg, &rc);

  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }
  
  // the bfp is turned into a serialized match object
  Blob blob = RDtreeSubset((uint8_t *)bfp.data(), bfp.size()).serialize();

  sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
}

/*
** A factory function for a tanimoto similarity search match object
*/
static void rdtree_tanimoto(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  int rc = SQLITE_OK;

  /* The first argument should be a bfp */
  std::string bfp = arg_to_bfp(argv[0], &rc);

  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  /* Check that the second argument is a float number */
  if (rc == SQLITE_OK && sqlite3_value_type(argv[1]) != SQLITE_FLOAT) {
    rc = SQLITE_MISMATCH;
  }

  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
    return;
  }

  // the bfp is turned into a serialized match object
  Blob blob = RDtreeTanimoto(
    (uint8_t *)bfp.data(), bfp.size(), sqlite3_value_double(argv[1])).serialize();

  sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
}

int chemicalite_init_rdtree(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) {
    rc = sqlite3_create_module_v2(db, "rdtree", &rdtreeModule, 
				0,  /* Client data for xCreate/xConnect */
				0   /* Module destructor function */
				);
  }

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "rdtree_subset", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<rdtree_subset>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "rdtree_tanimoto", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<rdtree_tanimoto>, 0, 0);

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "rdtree_link_index", 5, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, rdtree_link_index, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "rdtree_link_index", 6, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, rdtree_link_index, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "rdtree_unlink_index", 3, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, rdtree_unlink_index, 0, 0);

  return rc;
}