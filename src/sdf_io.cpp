#include <cassert>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <memory>

#include <GraphMol/FileParsers/MolSupplier.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"
#include "sdf_io.hpp"
#include "mol.hpp"

struct SdfReaderVtab : public sqlite3_vtab {
  std::string filename;
};

static int sdfReaderInit(sqlite3 *db, void */*pAux*/,
                      int argc, const char * const *argv,
                      sqlite3_vtab **ppVTab,
                      char **pzErr)
{
  if (argc != 4) {
    return SQLITE_ERROR;
  }

  std::string filename;
  std::istringstream filename_ss(argv[3]);
  filename_ss >> std::quoted(filename);

  int rc = sqlite3_declare_vtab(db, "CREATE TABLE x(molecule MOL)");

  if (rc == SQLITE_OK) {
    SdfReaderVtab *vtab = new SdfReaderVtab; // FIXME
    vtab->nRef = 0;
    vtab->pModule = 0;
    vtab->zErrMsg = 0;
    vtab->filename = filename;
    *ppVTab = (sqlite3_vtab *) vtab;
  }

  if (rc != SQLITE_OK) {
    *pzErr = sqlite3_mprintf("%s", sqlite3_errmsg(db));
  }

  return rc;
}

static int sdfReaderCreate(sqlite3 *db, void *pAux,
                      int argc, const char * const *argv,
                      sqlite3_vtab **ppVTab,
                      char **pzErr)
{
  return sdfReaderInit(db, pAux, argc, argv, ppVTab, pzErr);
}

static int sdfReaderConnect(sqlite3 *db, void *pAux,
                      int argc, const char * const *argv,
                      sqlite3_vtab **ppVTab,
                      char **pzErr)
{
  return sdfReaderInit(db, pAux, argc, argv, ppVTab, pzErr);
}

int sdfReaderBestIndex(sqlite3_vtab */*pVTab*/, sqlite3_index_info *pIndexInfo)
{
  /* A forward scan is the only supported mode, this method is therefore very minimal */
  pIndexInfo->estimatedCost = 100000; 
  return SQLITE_OK;
}

static int sdfReaderDisconnectDestroy(sqlite3_vtab *pVTab)
{
  SdfReaderVtab *vtab = (SdfReaderVtab *)pVTab;
  delete vtab;
  return SQLITE_OK;
}

struct SdfReaderCursor : public sqlite3_vtab_cursor {
  std::unique_ptr<RDKit::ForwardSDMolSupplier> supplier;
  sqlite3_int64 rowid;
  std::unique_ptr<RDKit::ROMol> mol;
};

static int sdfReaderOpen(sqlite3_vtab */*pVTab*/, sqlite3_vtab_cursor **ppCursor)
{
  int rc = SQLITE_OK;
  SdfReaderCursor *pCsr = new SdfReaderCursor; //FIXME 
  *ppCursor = (sqlite3_vtab_cursor *)pCsr;
  return rc;
}

static int sdfReaderClose(sqlite3_vtab_cursor *pCursor)
{
  SdfReaderCursor *p = (SdfReaderCursor *)pCursor;
  delete p;
  return SQLITE_OK;
}

static int sdfReaderFilter(sqlite3_vtab_cursor *pCursor, int /*idxNum*/, const char */*idxStr*/,
                     int /*argc*/, sqlite3_value **/*argv*/)
{
  SdfReaderCursor *p = (SdfReaderCursor *)pCursor;
  SdfReaderVtab *vtab = (SdfReaderVtab *)p->pVtab;

  std::unique_ptr<std::ifstream> pins(new std::ifstream(vtab->filename));
  if (!pins->is_open()) {
    // Maybe log something
    return SQLITE_ERROR;
  }

  p->supplier.reset(new RDKit::ForwardSDMolSupplier(pins.release(), true));
  if (!p->supplier->atEnd()) {
    p->rowid = 1;
    p->mol.reset(p->supplier->next());
  }

  return SQLITE_OK;
}

static int sdfReaderNext(sqlite3_vtab_cursor *pCursor)
{
  SdfReaderCursor * p = (SdfReaderCursor *)pCursor;
  if (!p->supplier->atEnd()) {
    p->rowid += 1;
    p->mol.reset(p->supplier->next());
  }
  return SQLITE_OK;
}

static int sdfReaderEof(sqlite3_vtab_cursor *pCursor)
{
  SdfReaderCursor * p = (SdfReaderCursor *)pCursor;
  return p->supplier->atEnd() ? 1 : 0;
}

static int sdfReaderColumn(sqlite3_vtab_cursor *pCursor, sqlite3_context *ctx, int N)
{
  SdfReaderCursor * p = (SdfReaderCursor *)pCursor;

  switch (N) {
    case 0:
      // the molecule
      if (p->mol) {
        int rc = SQLITE_OK;
        Blob blob = mol_to_blob(*p->mol, &rc);
        if (rc == SQLITE_OK) {
          sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
        }
        else {
          sqlite3_result_error_code(ctx, rc);
        }
      }
      else {
        sqlite3_result_null(ctx);
      }
      break;
    default:
      assert(!"unexpected column number");
      sqlite3_result_null(ctx);
  }
  return SQLITE_OK;
}

static int sdfReaderRowid(sqlite3_vtab_cursor *pCursor, sqlite_int64 *pRowid)
{
  SdfReaderCursor * p = (SdfReaderCursor *)pCursor;
  *pRowid = p->rowid;
  return SQLITE_OK;
}

/*
** The SDF reader module, collecting the methods that operate on the PeriodicTable vtab
*/
static sqlite3_module sdfReaderModule = {
  0,                           /* iVersion */
  sdfReaderCreate,             /* xCreate - create a table */ /* null because eponymous-only */
  sdfReaderConnect,            /* xConnect - connect to an existing table */
  sdfReaderBestIndex,          /* xBestIndex - Determine search strategy */
  sdfReaderDisconnectDestroy,  /* xDisconnect - Disconnect from a table */
  sdfReaderDisconnectDestroy,  /* xDestroy - Drop a table */
  sdfReaderOpen,               /* xOpen - open a cursor */
  sdfReaderClose,              /* xClose - close a cursor */
  sdfReaderFilter,             /* xFilter - configure scan constraints */
  sdfReaderNext,               /* xNext - advance a cursor */
  sdfReaderEof,                /* xEof */
  sdfReaderColumn,             /* xColumn - read data */
  sdfReaderRowid,              /* xRowid - read data */
  0,                           /* xUpdate - write data */
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

int chemicalite_init_sdf_io(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) {
    rc = sqlite3_create_module_v2(db, "sdf_reader", &sdfReaderModule, 
				  0,  /* Client data for xCreate/xConnect */
				  0   /* Module destructor function */
				  );
  }

  return rc;
}
