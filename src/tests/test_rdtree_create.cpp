#include "test_common.hpp"


TEST_CASE("rdtree create/drop", "[rdtree]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  SECTION("create and drop rdtree vtab")
  {
    int rc = sqlite3_exec(
        db, 
        "CREATE VIRTUAL TABLE xyz USING rdtree(id integer primary key, s bits(256))",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    test_select_value(db, "SELECT COUNT(*) FROM xyz_rowid", 0);
    test_select_value(db, "SELECT COUNT(*) FROM xyz_parent", 0);
    test_select_value(db, "SELECT COUNT(*) FROM xyz_node", 1);

    test_select_value(db, "SELECT COUNT(*) FROM xyz_bitfreq", 256);
    test_select_value(db, "SELECT COUNT(*) FROM xyz_weightfreq", 257);

    rc = sqlite3_exec(db, "DROP TABLE xyz", NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    sqlite3_stmt *pStmt;

    rc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM xyz_rowid", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_ERROR);
    rc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM xyz_parent", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_ERROR);
    rc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM xyz_node", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_ERROR);

    rc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM xyz_bitfreq", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_ERROR);
    rc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM xyz_weightfreq", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_ERROR);
  }

  test_db_close(db);
}
