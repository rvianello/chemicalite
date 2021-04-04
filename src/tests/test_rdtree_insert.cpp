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

  rc = sqlite3_exec(db, "DROP TABLE xyz", NULL, NULL, NULL);
  REQUIRE(rc == SQLITE_OK);

  test_db_close(db);
}
