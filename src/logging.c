#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "logging.h"
#include "settings.h"

#define LOG_BUFFER_SIZE 512

void chemicalite_log(int iErrCode, const char *zFormat, ...)
{
  char buffer[LOG_BUFFER_SIZE];

  va_list argp;
  va_start(argp, zFormat);
  sqlite3_vsnprintf(LOG_BUFFER_SIZE, buffer, zFormat, argp);
  va_end(argp);

  /* always send the info to the sqlite logger */
  sqlite3_log(iErrCode, buffer);

  ChemicaLiteOption option;
  int rc = chemicalite_get_option(LOGGING, &option);
  if (rc != SQLITE_OK) {
    sqlite3_log(SQLITE_INTERNAL, "Could not get chemicalite logging settings.");
  }

  FILE * out = NULL;
  switch (option) {
  case LOGGING_DISABLED:
    /* nothing to do */
    break;
  case LOGGING_STDOUT:
    out = stdout;
    break;
  case LOGGING_STDERR:
    out = stderr;
    break;
  default:
    assert(!"This should not happen");
    sqlite3_log(SQLITE_INTERNAL, "Invalide chemicalite logging settings");
  }
  if (out) {
    fprintf(out, "%s\n", buffer);
  }
}
