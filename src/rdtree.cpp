#include <cstring>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"
#include "rdtree.hpp"
#include "rdtree_vtab.hpp"

/* 
** RDtree virtual table module xDisconnect method.
*/
static int rdtreeDisconnect(sqlite3_vtab *vtab)
{
  RDtreeVtab *rdtree = (RDtreeVtab *)vtab;
  rdtree->decref();
  return SQLITE_OK;
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
** RDtree virtual table module xCreate method.
*/
static int rdtreeCreate(sqlite3 *db, void */*pAux*/,
			int argc, const char *const*argv,
			sqlite3_vtab **pvtab,
			char **pzErr)
{
  return RDtreeVtab::init(db, argc, argv, pvtab, pzErr, 1);
}

/* 
** RDtree virtual table module xConnect method.
*/
static int rdtreeConnect(sqlite3 *db, void */*pAux*/,
			 int argc, const char *const*argv,
			 sqlite3_vtab **pvtab,
			 char **pzErr)
{
  return RDtreeVtab::init(db, argc, argv, pvtab, pzErr, 0);
}

static sqlite3_module rdtreeModule = {
  0,                           /* iVersion */
  rdtreeCreate,                /* xCreate - create a table */
  rdtreeConnect,               /* xConnect - connect to an existing table */
  0, //rdtreeBestIndex,             /* xBestIndex - Determine search strategy */
  rdtreeDisconnect,            /* xDisconnect - Disconnect from a table */
  rdtreeDestroy,               /* xDestroy - Drop a table */
  0, //rdtreeOpen,                  /* xOpen - open a cursor */
  0, //rdtreeClose,                 /* xClose - close a cursor */
  0, //rdtreeFilter,                /* xFilter - configure scan constraints */
  0, //rdtreeNext,                  /* xNext - advance a cursor */
  0, //rdtreeEof,                   /* xEof */
  0, //rdtreeColumn,                /* xColumn - read data */
  0, //rdtreeRowid,                 /* xRowid - read data */
  0, //rdtreeUpdate,                /* xUpdate - write data */
  0,                           /* xBegin - begin transaction */
  0,                           /* xSync - sync transaction */
  0,                           /* xCommit - commit transaction */
  0,                           /* xRollback - rollback transaction */
  0,                           /* xFindFunction - function overloading */
  0, //rdtreeRename,                /* xRename - rename the table */
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