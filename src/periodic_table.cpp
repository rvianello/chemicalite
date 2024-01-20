#include <cassert>
#include <cstring>

#include <GraphMol/PeriodicTable.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "periodic_table.hpp"


static int pteConnect(sqlite3 *db, void */*pAux*/,
                      int /*argc*/, const char * const */*argv*/,
                      sqlite3_vtab **ppVTab,
                      char **pzErr)
{
  sqlite3_vtab *vtab = (sqlite3_vtab *)sqlite3_malloc(sizeof(sqlite3_vtab));

  if (!vtab) {
    return SQLITE_NOMEM;
  }
  memset(vtab, 0, sizeof(sqlite3_vtab));

  int rc = sqlite3_declare_vtab(db, "CREATE TABLE x("
    "atomic_number INTEGER, "
    "symbol TEXT, "
    "atomic_weight REAL, "
    "vdw_radius REAL, "
    "covalent_radius REAL, "
    "b0_radius REAL, "
    "default_valence INTEGER, "
    "n_outer_electrons INTEGER, "
    "most_common_isotope INTEGER, "
    "most_common_isotope_mass REAL "
    ")");

  if (rc != SQLITE_OK) {
      sqlite3_free(vtab);
      *pzErr = sqlite3_mprintf("%s", sqlite3_errmsg(db));
  }
  else {
    *ppVTab = vtab;
  }

  return rc;
}

int pteBestIndex(sqlite3_vtab */*pVTab*/, sqlite3_index_info *pIndexInfo)
{
  /* A forward scan is the only supported mode, this method is therefore very minimal */
  pIndexInfo->estimatedCost = 100000; 
  return SQLITE_OK;
}

static int pteDisconnect(sqlite3_vtab *pVTab)
{
  sqlite3_free(pVTab);
  return SQLITE_OK;
}

struct PteCursor {
  sqlite3_vtab_cursor base;
  sqlite3_int64 rowid;
};

static int pteOpen(sqlite3_vtab */*pVTab*/, sqlite3_vtab_cursor **ppCursor)
{
  int rc = SQLITE_NOMEM;
  PteCursor *pCsr;

  pCsr = (PteCursor *)sqlite3_malloc(sizeof(PteCursor));
  if (pCsr) {
    memset(pCsr, 0, sizeof(PteCursor));
    rc = SQLITE_OK;
  }
  *ppCursor = (sqlite3_vtab_cursor *)pCsr;

  return rc;
}

static int pteClose(sqlite3_vtab_cursor *pCursor)
{
  sqlite3_free(pCursor);
  return SQLITE_OK;
}

static int pteFilter(sqlite3_vtab_cursor *pCursor, int /*idxNum*/, const char */*idxStr*/,
                     int /*argc*/, sqlite3_value **/*argv*/)
{
  PteCursor *p = (PteCursor *)pCursor;
  p->rowid = 1;
  return SQLITE_OK;
}

static int pteNext(sqlite3_vtab_cursor *pCursor)
{
  PteCursor * p = (PteCursor *)pCursor;
  p->rowid += 1;
  return SQLITE_OK;
}

static int pteEof(sqlite3_vtab_cursor *pCursor)
{
  PteCursor *p = (PteCursor *)pCursor;
  return p->rowid >= 118+1; // is there a way to get this value programmatically?
}

static int pteColumn(sqlite3_vtab_cursor *pCursor, sqlite3_context *ctx, int N)
{
  PteCursor *p = (PteCursor *)pCursor;

  switch (N) {
    case 0:
      // atomic number
      sqlite3_result_int(ctx, p->rowid);
      break;
    case 1:
      // atomic symbol
      sqlite3_result_text(ctx, RDKit::PeriodicTable::getTable()->getElementSymbol(p->rowid).c_str(), -1, SQLITE_TRANSIENT);
      break;
    case 2:
      // atomic weight
      sqlite3_result_double(ctx, RDKit::PeriodicTable::getTable()->getAtomicWeight(p->rowid));
      break;
    case 3:
      // vdw radius
      sqlite3_result_double(ctx, RDKit::PeriodicTable::getTable()->getRvdw(p->rowid));
      break;
    case 4:
      // covalent radius
      sqlite3_result_double(ctx, RDKit::PeriodicTable::getTable()->getRcovalent(p->rowid));
      break;
    case 5:
      // b0 radius
      sqlite3_result_double(ctx, RDKit::PeriodicTable::getTable()->getRb0(p->rowid));
      break;
    case 6:
      // default valence
      sqlite3_result_int(ctx, RDKit::PeriodicTable::getTable()->getDefaultValence(p->rowid));
      break;
    case 7:
      // N outer electrons
      sqlite3_result_int(ctx, RDKit::PeriodicTable::getTable()->getNouterElecs(p->rowid));
      break;
    case 8:
      // most common isotope
      sqlite3_result_int(ctx, RDKit::PeriodicTable::getTable()->getMostCommonIsotope(p->rowid));
      break;
    case 9:
      // most common isotope mass
      sqlite3_result_double(ctx, RDKit::PeriodicTable::getTable()->getMostCommonIsotopeMass(p->rowid));
      break;
    default:
      assert(!"Unexpected column number");
      sqlite3_result_null(ctx);
  }
  return SQLITE_OK;
}

static int pteRowid(sqlite3_vtab_cursor *pCursor, sqlite_int64 *pRowid)
{
  PteCursor * p = (PteCursor *)pCursor;
  *pRowid = p->rowid;
  return SQLITE_OK;
}

/*
** The periodic table module, collecting the methods that operate on the PeriodicTable vtab
*/
static sqlite3_module pteModule = {
  3,                           /* iVersion */
  0,                           /* xCreate - create a table */ /* null because eponymous-only */
  pteConnect,                  /* xConnect - connect to an existing table */
  pteBestIndex,                /* xBestIndex - Determine search strategy */
  pteDisconnect,               /* xDisconnect - Disconnect from a table */
  0,                           /* xDestroy - Drop a table */
  pteOpen,                     /* xOpen - open a cursor */
  pteClose,                    /* xClose - close a cursor */
  pteFilter,                   /* xFilter - configure scan constraints */
  pteNext,                     /* xNext - advance a cursor */
  pteEof,                      /* xEof */
  pteColumn,                   /* xColumn - read data */
  pteRowid,                    /* xRowid - read data */
  0, //pteUpdate,                   /* xUpdate - write data */
  0,                           /* xBegin - begin transaction */
  0,                           /* xSync - sync transaction */
  0,                           /* xCommit - commit transaction */
  0,                           /* xRollback - rollback transaction */
  0,                           /* xFindFunction - function overloading */
  0,                           /* xRename - rename the table */
  0,                           /* xSavepoint */
  0,                           /* xRelease */
  0,                           /* xRollbackTo */
  0                            /* xShadowName */
};

int chemicalite_init_periodic_table(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) {
    rc = sqlite3_create_module_v2(db, "periodic_table", &pteModule, 
				  0,  /* Client data for xCreate/xConnect */
				  0   /* Module destructor function */
				  );
  }

  return rc;
}
