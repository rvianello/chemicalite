#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "rdtree.h"


static sqlite3_module rdtreeModule = {
  0,                           /* iVersion */
  /* rdtreeCreate,                /* xCreate - create a table */
  /* rdtreeConnect,               /* xConnect - connect to an existing table */
  /* rdtreeBestIndex,             /* xBestIndex - Determine search strategy */
  /* rdtreeDisconnect,            /* xDisconnect - Disconnect from a table */
  /* rdtreeDestroy,               /* xDestroy - Drop a table */
  /* rdtreeOpen,                  /* xOpen - open a cursor */
  /* rdtreeClose,                 /* xClose - close a cursor */
  /* rdtreeFilter,                /* xFilter - configure scan constraints */
  /* rdtreeNext,                  /* xNext - advance a cursor */
  /* rdtreeEof,                   /* xEof */
  /* rdtreeColumn,                /* xColumn - read data */
  /* rdtreeRowid,                 /* xRowid - read data */
  /* rdtreeUpdate,                /* xUpdate - write data */
  0,                           /* xBegin - begin transaction */
  0,                           /* xSync - sync transaction */
  0,                           /* xCommit - commit transaction */
  0,                           /* xRollback - rollback transaction */
  0,                           /* xFindFunction - function overloading */
  /* rdtreeRename,                /* xRename - rename the table */
  0,                           /* xSavepoint */
  0,                           /* xRelease */
  0                            /* xRollbackTo */
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
