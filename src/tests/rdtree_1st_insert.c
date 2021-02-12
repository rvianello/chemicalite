#include <stdio.h>

#include "testcommon.h"

/*
** verify succesful insertion of a molecular signature into a newly created
** rdtree virtual table.
*/
int main()
{
  int rc = SQLITE_OK;
  char *errMsg = 0;
  sqlite3 *db = 0;
  int closedb = 1;
  int rowid_count;
  int parent_count;
  int node_count;

  char sql1[] = "select count(*) from xyz_rowid";
  char sql2[] = "select count(*) from xyz_parent";
  char sql3[] = "select count(*) from xyz_node";

  if ((rc = database_setup(":memory:", &db, &errMsg)) != SQLITE_OK) {
    closedb = 0;
    printf("Failed database initialization\n");
  }
  else if ((rc = create_rdtree(db, "xyz", 32, &errMsg)) != SQLITE_OK) {
    printf("Failed creation of virtual rdtree table\n");
  }
  else if ((rc = insert_bitstring(db, "xyz", -1, 
				  "\x01\x02\x04\x08", 4)) != SQLITE_OK) {
    printf("Failed insertion\n");
  }
  else if ((rc = select_integer(db, sql1, &rowid_count)) != SQLITE_OK) {
    printf("Couldn't query rowid table\n");
  }
  else if ((rc = select_integer(db, sql2, &parent_count)) != SQLITE_OK) {
    printf("Couldn't query parent table\n");
  }
  else if ((rc = select_integer(db, sql3, &node_count)) != SQLITE_OK) {
    printf("Couldn't query node table\n");
  }
  else if (rowid_count != 1) {
    printf("Unexpected number of rowid records\n");
    rc = SQLITE_MISMATCH;
  }
  else if (parent_count != 0) {
    printf("Unexpected number of parent records\n");
    rc = SQLITE_MISMATCH;
  }
  else if (node_count != 1) {
    printf("Unexpected number of node records\n");
    rc = SQLITE_MISMATCH;
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
