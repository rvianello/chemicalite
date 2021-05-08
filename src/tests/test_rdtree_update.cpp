#include "test_common.hpp"

TEST_CASE("rdtree update", "[rdtree]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  int rc = sqlite3_exec(
      db, 
      "CREATE VIRTUAL TABLE xyz USING rdtree(id integer primary key, s bits(1024))",
      NULL, NULL, NULL);
  REQUIRE(rc == SQLITE_OK);

  SECTION("Test GitHub #00003") {
    // insert a first bfp (any bfp would do, but the original github
    // ticket used an empty one)
    rc = sqlite3_exec(
        db, 
        "INSERT INTO xyz(id, s) VALUES(1, bfp_dummy(1024, 0))",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    // verify that the bfp looks good
    test_select_value(
      db, 
      "SELECT bfp_weight(s) FROM xyz WHERE id=1", 0);

    // and then update the record with a different bfp
    rc = sqlite3_exec(
        db, 
        "UPDATE xyz SET s=bfp_dummy(1024, 1) WHERE id=1",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    // verify that the bfp was updated
    test_select_value(
      db, 
      "SELECT bfp_weight(s) FROM xyz WHERE id=1", 1024/8);
  }

  SECTION("Test GitHub #00003-bis") {
    // insert a first bfp (any bfp would do, but the original github
    // ticket used an empty one)
    rc = sqlite3_exec(
        db, 
        "INSERT INTO xyz(id, s) VALUES(1, bfp_dummy(1024, 0))",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    // insert a second bfp
    rc = sqlite3_exec(
        db, 
        "INSERT INTO xyz(id, s) VALUES(2, bfp_dummy(1024, 2))",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    // verify that the first bfp looks good
    test_select_value(
      db, 
      "SELECT bfp_weight(s) FROM xyz WHERE id=1", 0);

    // and then update the first record with a different bfp
    rc = sqlite3_exec(
        db, 
        "UPDATE xyz SET s=bfp_dummy(1024, 1) WHERE id=1",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    // verify that the bfp was updated
    test_select_value(
      db, 
      "SELECT bfp_weight(s) FROM xyz WHERE id=1", 1024/8);
  }

  SECTION("Test GitHub #00003-ter") {
    // insert a first bfp (any bfp would do, but the original github
    // ticket used an empty one)
    rc = sqlite3_exec(
        db, 
        "INSERT INTO xyz(id, s) VALUES(1, bfp_dummy(1024, 0))",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    // insert a second bfp
    rc = sqlite3_exec(
        db, 
        "INSERT INTO xyz(id, s) VALUES(2, bfp_dummy(1024, 2))",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    // verify that the second bfp looks good
    test_select_value(
      db, 
      "SELECT bfp_weight(s) FROM xyz WHERE id=2", 1024/8);

    // and then update the second (and last inserted) record with a different bfp
    rc = sqlite3_exec(
        db, 
        "UPDATE xyz SET s=bfp_dummy(1024, 3) WHERE id=2",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    // verify that the bfp was updated
    test_select_value(
      db, 
      "SELECT bfp_weight(s) FROM xyz WHERE id=2", 2*1024/8);
  }

  rc = sqlite3_exec(db, "DROP TABLE xyz", NULL, NULL, NULL);
  REQUIRE(rc == SQLITE_OK);

  test_db_close(db);
}
