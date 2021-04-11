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

    test_select_value(db, "SELECT COUNT(*) FROM xyz", 1);

    sqlite3_stmt *pStmt = 0;

    // verify we get back a wrapped bfp type 
    rc = sqlite3_prepare(db, "SELECT s FROM xyz WHERE id=1", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    rc = sqlite3_step(pStmt);
    REQUIRE(rc == SQLITE_ROW);

    int bytes = sqlite3_column_bytes(pStmt, 0);
    REQUIRE(bytes == sizeof(uint32_t)+1024/8);

    sqlite3_finalize(pStmt);

    // verify the s behaves like a regular bfp column
    rc = sqlite3_prepare(db, "SELECT bfp_length(s) FROM xyz WHERE id=1", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    rc = sqlite3_step(pStmt);
    REQUIRE(rc == SQLITE_ROW);

    int type = sqlite3_column_type(pStmt, 0);
    REQUIRE(type == SQLITE_INTEGER);

    int length = sqlite3_column_int(pStmt, 0);
    REQUIRE(length == 1024); // bfp_length returns bits

    sqlite3_finalize(pStmt);

  }

  SECTION("insert multiple bfp values")
  {
    const int NUM_BFPS = 42;

    sqlite3_stmt *pStmt = 0;
    int rc = sqlite3_prepare(db, "INSERT INTO xyz(id, s) VALUES(?1, bfp_dummy(1024, ?2))", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    for (int i=0; i < NUM_BFPS; ++i) {
      rc = sqlite3_bind_int(pStmt, 1, i+1);
      REQUIRE(rc == SQLITE_OK);

      rc = sqlite3_bind_int(pStmt, 2, i+1);
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

    // verify we get back a wrapped bfp type 
    rc = sqlite3_prepare(db, "SELECT s FROM xyz WHERE id=16", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    rc = sqlite3_step(pStmt);
    REQUIRE(rc == SQLITE_ROW);

    int bytes = sqlite3_column_bytes(pStmt, 0);
    REQUIRE(bytes == sizeof(uint32_t)+1024/8);

    sqlite3_finalize(pStmt);

    // verify the s behaves like a regular bfp column
    rc = sqlite3_prepare(db, "SELECT bfp_length(s) FROM xyz WHERE id=16", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    rc = sqlite3_step(pStmt);
    REQUIRE(rc == SQLITE_ROW);

    int length_type = sqlite3_column_type(pStmt, 0);
    REQUIRE(length_type == SQLITE_INTEGER);

    int length = sqlite3_column_int(pStmt, 0);
    REQUIRE(length == 1024); // bfp_length returns bits

    sqlite3_finalize(pStmt);

    // verify the s behaves like a regular bfp column
    rc = sqlite3_prepare(db, "SELECT bfp_weight(s) FROM xyz WHERE id=16", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    rc = sqlite3_step(pStmt);
    REQUIRE(rc == SQLITE_ROW);

    int weight_type = sqlite3_column_type(pStmt, 0);
    REQUIRE(weight_type == SQLITE_INTEGER);

    int weight = sqlite3_column_int(pStmt, 0);
    REQUIRE(weight == 1024/8); // each byte has 1 bit set, because for id=16 we inserted bfp_dummy(1024, 16) 

    sqlite3_finalize(pStmt);
  }

  rc = sqlite3_exec(db, "DROP TABLE xyz", NULL, NULL, NULL);
  REQUIRE(rc == SQLITE_OK);

  test_db_close(db);
}
