#include <cassert>
#include <cstring>
#include <fstream>
#include <string>
#include <memory>

#include <GraphMol/FileParsers/MolSupplier.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"
#include "sdf_io.hpp"
#include "mol.hpp"

// static const int SDF_INDEX_COLUMN = 0;
// static const int SDF_MOLECULE_COLUMN = 1;
static const int SDF_FILEPATH_COLUMN = 2;

static int sdfReaderConnect(sqlite3 *db, void */*pAux*/,
                      int /*argc*/, const char * const */*argv*/,
                      sqlite3_vtab **ppVTab,
                      char **pzErr)
{
  int rc = sqlite3_declare_vtab(db, "CREATE TABLE x("
    "idx INTEGER, molecule MOL, "
    "file_path HIDDEN"
    ")");

  if (rc == SQLITE_OK) {
    sqlite3_vtab *vtab = *ppVTab = (sqlite3_vtab *) sqlite3_malloc(sizeof(sqlite3_vtab));
    if (!vtab) {
      rc = SQLITE_NOMEM;
    }
    else {
      memset(vtab, 0, sizeof(sqlite3_vtab));
    }
  }

  if (rc != SQLITE_OK) {
    *pzErr = sqlite3_mprintf("%s", sqlite3_errmsg(db));
  }

  return rc;
}

int sdfReaderBestIndex(sqlite3_vtab */*pVTab*/, sqlite3_index_info *pIndexInfo)
{
  // At this time the only arg passed to the SDF parser is the input file path
  // this function is consequently very simple. it might become more complex
  // when/if support for more args/options is implemented. 
  int file_path_index = -1;
  for (int index = 0; index < pIndexInfo->nConstraint; ++index) {
    if (pIndexInfo->aConstraint[index].usable == 0) {
      continue;
    }
    if (pIndexInfo->aConstraint[index].iColumn == SDF_FILEPATH_COLUMN &&
        pIndexInfo->aConstraint[index].op == SQLITE_INDEX_CONSTRAINT_EQ) {
      file_path_index = index;
    }
  }
  if (file_path_index < 0) {
    // The SDF file path is not available, or it's not usable,
    // This plan is unusable for this table
    return SQLITE_CONSTRAINT;
  }
  pIndexInfo->idxNum = 1; // Not really meaningful a this time
  pIndexInfo->aConstraintUsage[file_path_index].argvIndex = 1; // will be argv[0] for xFilter
  pIndexInfo->aConstraintUsage[file_path_index].omit = 1; // no need for SQLite to verify
  pIndexInfo->estimatedCost = 1000000; 
  return SQLITE_OK;
}

static int sdfReaderDisconnect(sqlite3_vtab *pVTab)
{
  sqlite3_free(pVTab);
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
                     int argc, sqlite3_value **argv)
{
  SdfReaderCursor *p = (SdfReaderCursor *)pCursor;

  if (argc != 1) {
    // at present we always expect the input file path as the only arg
    return SQLITE_ERROR;
  }

  sqlite3_value *arg = argv[0];
  if (sqlite3_value_type(arg) != SQLITE_TEXT) {
    return SQLITE_MISMATCH;
  }

  const char * file_path = (const char *) sqlite3_value_text(arg);
  std::unique_ptr<std::ifstream> pins(new std::ifstream(file_path));
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
      // row id
      sqlite3_result_int(ctx, p->rowid);
      break;
    case 1:
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
  0,                           /* xCreate - create a table */ /* null because eponymous-only */
  sdfReaderConnect,            /* xConnect - connect to an existing table */
  sdfReaderBestIndex,          /* xBestIndex - Determine search strategy */
  sdfReaderDisconnect,         /* xDisconnect - Disconnect from a table */
  0,                           /* xDestroy - Drop a table */
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
