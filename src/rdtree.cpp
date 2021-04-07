#include <cstring>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"
#include "rdtree.hpp"
#include "rdtree_vtab.hpp"

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
  0,                           /* iVersion */
  rdtreeCreate,                /* xCreate - create a table */
  rdtreeConnect,               /* xConnect - connect to an existing table */
  rdtreeBestIndex,             /* xBestIndex - Determine search strategy */
  rdtreeDisconnect,            /* xDisconnect - Disconnect from a table */
  rdtreeDestroy,               /* xDestroy - Drop a table */
  rdtreeOpen,                  /* xOpen - open a cursor */
  rdtreeClose,                 /* xClose - close a cursor */
  0, //rdtreeFilter,                /* xFilter - configure scan constraints */
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
};

int chemicalite_init_rdtree(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) {
    rc = sqlite3_create_module_v2(db, "rdtree", &rdtreeModule, 
				0,  /* Client data for xCreate/xConnect */
				0   /* Module destructor function */
				);
  }

  return rc;
}