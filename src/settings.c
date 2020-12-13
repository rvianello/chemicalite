#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "settings.h"
#include "utils.h"

enum SettingType { OPTION, INTEGER, REAL };
typedef enum SettingType SettingType;

struct Setting {
  char key[16];
  SettingType type;
  union {
    ChemicaLiteOption option;
    int32_t integer;
    double real;
  };
};
typedef struct Setting Setting;

static Setting settings[] = {
  { "logging", OPTION, { .option = LOGGING_DISABLED } }
#ifdef ENABLE_TEST_SETTINGS
  ,
  { "answer", INTEGER, { .integer = 42 } },
  { "pi", REAL, { .real = 3.14 } }
#endif
};

const char * chemicalite_option_label(ChemicaLiteOption option)
{
  static const char * labels[] = {
    "disabled",
    "stdout",
    "stderr"
  };

  assert(sizeof labels / sizeof labels[0] == CHEMICALITE_NUM_OPTIONS);
  return labels[option];
}

/*
** Settings getters and setters
*/
int chemicalite_set_option(ChemicaliteSetting setting, ChemicaLiteOption value)
{
  if (settings[setting].type != OPTION) {
    return SQLITE_MISMATCH;
  }

  /* placing the validation here is not ideal and not very sustainable, but for now it's fine */
  if (setting == LOGGING && value != LOGGING_DISABLED && value != LOGGING_STDOUT && value != LOGGING_STDERR) {
    return SQLITE_MISMATCH;
  }

  settings[setting].option = value;
  return SQLITE_OK;
}

int chemicalite_get_option(ChemicaliteSetting setting, ChemicaLiteOption *pValue)
{
  if (settings[setting].type != OPTION) {
    return SQLITE_MISMATCH;
  }
  *pValue = settings[setting].option;
  return SQLITE_OK;
}

int chemicalite_set_int(ChemicaliteSetting setting, int value)
{
  if (settings[setting].type != INTEGER) {
    return SQLITE_MISMATCH;
  }

  settings[setting].integer = value;
  return SQLITE_OK;
}

int chemicalite_get_int(ChemicaliteSetting setting, int *pValue)
{
  if (settings[setting].type != INTEGER) {
    return SQLITE_MISMATCH;
  }
  *pValue = settings[setting].integer;
  return SQLITE_OK;
}

int chemicalite_set_double(ChemicaliteSetting setting, double value)
{
  if (settings[setting].type != REAL) {
    return SQLITE_MISMATCH;
  }

  settings[setting].real = value;
  return SQLITE_OK;
}

int chemicalite_get_double(ChemicaliteSetting setting, double *pValue)
{
  if (settings[setting].type != REAL) {
    return SQLITE_MISMATCH;
  }
  *pValue = settings[setting].real;
  return SQLITE_OK;
}

/*
** The settings virtual table object
*/
struct SettingsTable {
  sqlite3_vtab base;
};
typedef struct SettingsTable SettingsTable;


static int settingsConnect(sqlite3 *db, void *pAux,
                           int argc, const char * const *argv,
                           sqlite3_vtab **ppVTab,
                           char **pzErr)
{
  UNUSED(db);
  UNUSED(pAux);
  UNUSED(argc);
  UNUSED(argv);
  UNUSED(pzErr);
  
  assert(sizeof settings / sizeof settings[0] == CHEMICALITE_NUM_SETTINGS);

  SettingsTable *pSettings = (SettingsTable *) sqlite3_malloc(sizeof(SettingsTable));

  if (!pSettings) {
    return SQLITE_NOMEM;
  }
  memset(pSettings, 0, sizeof(SettingsTable));

  int rc = sqlite3_declare_vtab(db, "CREATE TABLE x(key, value)");

  if (rc != SQLITE_OK) {
      sqlite3_free(pSettings);
      *pzErr = sqlite3_mprintf("%s", sqlite3_errmsg(db));
  }
  else {
    *ppVTab = (sqlite3_vtab *)pSettings;
  }

  return rc;
}

int settingsBestIndex(sqlite3_vtab *pVTab, sqlite3_index_info *pIndexInfo)
{
  UNUSED(pVTab);
  /* A forward scan is the only supported mode, this method is therefore very minimal */
  pIndexInfo->estimatedCost = 100000; 
  return SQLITE_OK;
}

static int settingsDestroyDisconnect(sqlite3_vtab *pVTab)
{
  sqlite3_free((SettingsTable *)pVTab);
  return SQLITE_OK;
}

struct SettingsCursor {
  sqlite3_vtab_cursor base;
  sqlite3_int64 rowid;
};
typedef struct SettingsCursor SettingsCursor;

static int settingsOpen(sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor)
{
  UNUSED(pVTab);
  int rc = SQLITE_NOMEM;
  SettingsCursor *pCsr;

  pCsr = (SettingsCursor *)sqlite3_malloc(sizeof(SettingsCursor));
  if (pCsr) {
    memset(pCsr, 0, sizeof(SettingsCursor));
    rc = SQLITE_OK;
  }
  *ppCursor = (sqlite3_vtab_cursor *)pCsr;

  return rc;
}

static int settingsClose(sqlite3_vtab_cursor *pCursor)
{
  sqlite3_free(pCursor);
  return SQLITE_OK;
}

static int settingsFilter(sqlite3_vtab_cursor *pCursor, int idxNum, const char *idxStr,
                          int argc, sqlite3_value **argv)
{
  UNUSED(idxNum);
  UNUSED(idxStr);
  UNUSED(argc);
  UNUSED(argv);

  SettingsCursor *p = (SettingsCursor *)pCursor;
  p->rowid = 0;

  return SQLITE_OK;
}

static int settingsEof(sqlite3_vtab_cursor *pCursor)
{
  SettingsCursor *p = (SettingsCursor *)pCursor;
  return p->rowid >= CHEMICALITE_NUM_SETTINGS;
}

static int settingsNext(sqlite3_vtab_cursor *pCursor)
{
  SettingsCursor * p = (SettingsCursor *)pCursor;
  p->rowid += 1;
  return SQLITE_OK;
}

static int settingsColumn(sqlite3_vtab_cursor *pCursor, sqlite3_context *ctx, int N)
{
  SettingsCursor *p = (SettingsCursor *)pCursor;

  if (N == 0) {
    sqlite3_result_text(ctx, settings[p->rowid].key, -1, 0);
  }
  else if (N == 1) {
    switch (settings[p->rowid].type) {
    case OPTION: 
      sqlite3_result_text(
        ctx, chemicalite_option_label(settings[p->rowid].option), -1, 0);
      break;
    case INTEGER:
      sqlite3_result_int(ctx, settings[p->rowid].integer);
      break;
    case REAL:
      sqlite3_result_double(ctx, settings[p->rowid].real);
      break;
    default:
      assert(!"Unexpected value type for option");
      sqlite3_result_null(ctx);
    }
  }
  return SQLITE_OK;
}

static int settingsRowid(sqlite3_vtab_cursor *pCursor, sqlite_int64 *pRowid)
{
  SettingsCursor * p = (SettingsCursor *)pCursor;
  *pRowid = p->rowid;
  return SQLITE_OK;
}

static int settingsUpdate(sqlite3_vtab *pVTab, int argc, sqlite3_value **argv, sqlite_int64 *pRowid)
{
  UNUSED(pVTab);
  UNUSED(pRowid);

  if (argc == 1) {
    /* a pure delete operation, not allowed */
    return SQLITE_MISUSE;
  }

  if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
    /* insert of a new row, also not allowed */
    return SQLITE_MISUSE;
  }

  int64_t rowid = sqlite3_value_int64(argv[0]);

  if (rowid != sqlite3_value_int64(argv[1])) {
    /* update w/ roid replacement, not allowed */
    return SQLITE_CONSTRAINT;
  }

  if (rowid >= CHEMICALITE_NUM_SETTINGS) {
    assert(!"this should never happen");
    return SQLITE_CONSTRAINT;
  }

  /* now argv[2] is the key column, argv[3] the value column */
  if ( (sqlite3_value_type(argv[2]) != SQLITE_TEXT) ||
       strcmp((const char *)sqlite3_value_text(argv[2]), settings[rowid].key) ) {
    /* modifying the settings keys is not allowed */
    return SQLITE_CONSTRAINT;
  }

  int rc = SQLITE_OK;
  int value_type = sqlite3_value_type(argv[3]);

  switch (value_type)
  {
  case SQLITE_INTEGER:
    rc = chemicalite_set_int(rowid, sqlite3_value_int(argv[3]));
    break;
  case SQLITE_FLOAT:
    rc = chemicalite_set_double(rowid, sqlite3_value_double(argv[3]));
    break;
  case SQLITE_TEXT:
    ; /* add an empty statement after the case label, because a declaration is following */
    ChemicaLiteOption option = CHEMICALITE_NUM_OPTIONS; /* initialize w/ invalid value */
    const char * value = (const char *) sqlite3_value_text(argv[3]);
    for (int i = 0; i < CHEMICALITE_NUM_OPTIONS; ++i) {
        if (!strcasecmp(value, chemicalite_option_label(i))) {
            option = i;
            break;
        }
    }
    if (option == CHEMICALITE_NUM_OPTIONS) {
        /* still invalid, we were passed an input string that doesn't match any known option*/
        rc = SQLITE_CONSTRAINT;
    }
    else {
        rc = chemicalite_set_option(rowid, option);
    }
    break;
  default:
    rc = SQLITE_MISMATCH;
    break;
  }

  return rc;
}

/*
** The settings module, collecting the methods that operate on the Settings vtab
*/
static sqlite3_module settingsModule = {
  0,                           /* iVersion */
  0,                           /* xCreate - create a table */ /* null because eponymous-only */
  settingsConnect,             /* xConnect - connect to an existing table */
  settingsBestIndex,           /* xBestIndex - Determine search strategy */
  settingsDestroyDisconnect,   /* xDisconnect - Disconnect from a table */
  settingsDestroyDisconnect,   /* xDestroy - Drop a table */
  settingsOpen,                /* xOpen - open a cursor */
  settingsClose,               /* xClose - close a cursor */
  settingsFilter,              /* xFilter - configure scan constraints */
  settingsNext,                /* xNext - advance a cursor */
  settingsEof,                 /* xEof */
  settingsColumn,              /* xColumn - read data */
  settingsRowid,               /* xRowid - read data */
  settingsUpdate,              /* xUpdate - write data */
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


int chemicalite_init_settings(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) {
    rc = sqlite3_create_module_v2(db, "chemicalite_settings", &settingsModule, 
				  0,  /* Client data for xCreate/xConnect */
				  0   /* Module destructor function */
				  );
  }

  return rc;
}
