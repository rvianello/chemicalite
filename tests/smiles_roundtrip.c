#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "testcommon.h"

/*
** perform an extremely basic test performing the conversion 
** from SMILES to mol and from mol to SMILES again.
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
    printf("At line %d: SQLite error %d", __LINE__, rc);
    return rc;
  }

  const char * txt = NULL;

  /* this time skip the explicit conversion to mol */
  rc = select_text(db, "SELECT mol_smiles('CCCC')", &txt);

  if (SQLITE_OK != rc) {
    printf("At line %d: SQLite error %d", __LINE__, rc);
    return rc;
  }

  if (strcmp(txt, "CCCC")) {
    printf("At line %d: Got %s, CCCC expected", __LINE__, txt);
    rc = SQLITE_ERROR;
  }

  free((void *)txt);

  if (SQLITE_OK != rc) {
    return rc;
  }

  /* perform the explicit conversion to mol */
  rc = select_text(db, "SELECT mol_smiles(mol('CCCC'))", &txt);

  if (SQLITE_OK != rc) {
    printf("At line %d: SQLite error %d", __LINE__, rc);
  }

  if (SQLITE_OK == rc && strcmp(txt, "CCCC")) {
    printf("At line %d: Got %s, CCCC expected", __LINE__, txt);
    rc = SQLITE_ERROR;
  }

  free((void *)txt);

  int rc2 = sqlite3_close(db);

  if (SQLITE_OK == rc) {
    rc = rc2;
  }

  return rc;
}
