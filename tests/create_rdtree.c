#include <stdio.h>

#include "testcommon.h"

/*
** verify rdtree virtual table and storage backend are created
*/
int main()
{
  int rc = SQLITE_OK;
  char *errMsg = 0;
  sqlite3 *db = 0;
  int closedb = 1;
  int dummy;

  char sql1[] = "select count(*) from xyz_rowid";
  char sql2[] = "select count(*) from xyz_parent";
  char sql3[] = "select count(*) from xyz_node";

  if ((rc = database_setup(":memory:", &db, &errMsg)) != SQLITE_OK) {
    closedb = 0;
    printf("Failed database initialization\n");
  }
  else if ((rc = create_rdtree(db, "xyz", 128, &errMsg)) != SQLITE_OK) {
    printf("Failed creation of virtual rdtree table\n");
  }
  else if ((rc = select_integer(db, sql1, &dummy)) != SQLITE_OK) {
    printf("Couldn't query rowid table\n");
  }
  else if ((rc = select_integer(db, sql2, &dummy)) != SQLITE_OK) {
    printf("Couldn't query parent table\n");
  }
  else if ((rc = select_integer(db, sql3, &dummy)) != SQLITE_OK) {
    printf("Couldn't query node table\n");
  }
  else if (create_rdtree_ex(db, "abc", 128, "WHATEVER", &errMsg) == SQLITE_OK) {
    rc = 42;
    printf("Successful creation of virtual rdtree table "
	   "w/ options 'WHATEVER'\n");
  }
  else if ((rc = create_rdtree_ex(db, "def", 128, "OPT_FOR_SUBSET_QUERIES",
				  &errMsg))
	    != SQLITE_OK) {
    printf("Couldn't create virtual rdtree table "
	   "w/ options 'OPT_FOR_SUBSET_QUERIES'\n");
  }
  else if ((rc = create_rdtree_ex(db, "ghi", 128, "OPT_FOR_SIMILARITY_QUERIES",
				  &errMsg))
	    != SQLITE_OK) {
    printf("Couldn't create virtual rdtree table "
	   "w/ options 'OPT_FOR_SIMILARITY_QUERIES'\n");
  }
  
  if (closedb) {
    int rc2 = sqlite3_close(db);
    if (SQLITE_OK == rc) { rc = rc2; }
  }

  if (errMsg) {
    printf("%s\n", errMsg);
    sqlite3_free(errMsg);
  }

  return rc;
}
