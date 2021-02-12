#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "testcommon.h"

/*
** verify the extension is correctly loaded
*/
int main()
{
  char *errMsg = 0;
  sqlite3 *db = 0;

  int rc = database_setup(":memory:", &db, &errMsg);

  if (errMsg) {
    printf("%s\n", errMsg);
    sqlite3_free(errMsg);
  }

  if (SQLITE_OK != rc) {
    return rc;
  }

  const char * txt = NULL;

  rc = select_text(db, "SELECT chemicalite_version()", &txt);

  if (SQLITE_OK == rc && strcmp(txt, XSTRINGIFY(CHEMICALITE_VERSION))) {
    rc = SQLITE_ERROR;
  }

  free((void *)txt);
  txt = NULL;
  
  /* For RDKit and Boost only exercise the functions, w/out checking the output */
  if (SQLITE_OK == rc) {
    rc = select_text(db, "SELECT rdkit_version()", &txt);
  }

  free((void *)txt);
  txt = NULL;

  if (SQLITE_OK == rc) {
      rc = select_text(db, "SELECT boost_version()", &txt);
  }

  free((void *)txt);

  int rc2 = sqlite3_close(db);

  if (SQLITE_OK == rc) {
    rc = rc2;
  }

  return rc;
}
