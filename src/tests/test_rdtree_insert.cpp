#include "test_common.hpp"

TEST_CASE("rdtree insert", "[rdtree]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  int rc = sqlite3_exec(
      db, 
      "CREATE VIRTUAL TABLE xyz USING rdtree(id integer primary key, s bits(1024))",
      NULL, NULL, NULL);
  REQUIRE(rc == SQLITE_OK);

  SECTION("1st insert")
  {
    rc = sqlite3_exec(
        db, 
        "INSERT INTO xyz(s) VALUES(bfp_dummy(1024, 42))",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    test_select_value(db, "SELECT COUNT(*) FROM xyz_rowid", 1);
    test_select_value(db, "SELECT COUNT(*) FROM xyz_parent", 0);
    test_select_value(db, "SELECT COUNT(*) FROM xyz_node", 1);
  }

  SECTION("insert multiple bfp values")
  {
    const int NUM_BFPS = 42;

    sqlite3_stmt *pStmt = 0;
    int rc = sqlite3_prepare(db, "INSERT INTO xyz(s) VALUES(bfp_dummy(1024, ?1))", -1, &pStmt, 0);

    for (int i=0; i < NUM_BFPS; ++i) {
      rc = sqlite3_bind_int(pStmt, 1, i);
      REQUIRE(rc == SQLITE_OK);

      rc = sqlite3_step(pStmt);
      REQUIRE(rc == SQLITE_DONE);

      rc = sqlite3_reset(pStmt);
      REQUIRE(rc == SQLITE_OK);
    }

    sqlite3_finalize(pStmt);

    test_select_value(db, "SELECT COUNT(*) FROM xyz_rowid", NUM_BFPS);
    test_select_value(db, "SELECT COUNT(*) FROM xyz_parent", 2);
    test_select_value(db, "SELECT COUNT(*) FROM xyz_node", 3);
  }

  rc = sqlite3_exec(db, "DROP TABLE xyz", NULL, NULL, NULL);
  REQUIRE(rc == SQLITE_OK);

  test_db_close(db);
}
