#include <stdio.h>

#include "testcommon.h"

/*
** verify the extension is correctly loaded
*/
int main(int argc, char *argv[])
{
  char *errMsg = 0;
  sqlite3 *db = 0;

  int rc = database_setup(":memory:", argv[1], &db, &errMsg);

  if (SQLITE_OK == rc) {
    rc = sqlite3_close(db);
  }

  if (errMsg) {
    printf("%s\n", errMsg);
    sqlite3_free(errMsg);
  }

  return rc;
}
