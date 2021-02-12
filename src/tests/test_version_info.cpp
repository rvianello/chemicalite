#include <sqlite3.h>
#include <catch2/catch.hpp>

#include <RDGeneral/versions.h>

#include "../utils.hpp"

TEST_CASE("ChemicaLite + RDKit version info", "[smoke]")
{
  sqlite3 * db = nullptr;
  int rc = SQLITE_OK;

  // Create a connection to an in-memory database
  rc = sqlite3_open(":memory:", &db);
  REQUIRE(rc == SQLITE_OK);

  // Enable loading extensions
  rc = sqlite3_enable_load_extension(db, 1);
  REQUIRE(rc == SQLITE_OK);

  // Load ChemicaLite
  rc = sqlite3_load_extension(db, "chemicalite", 0, 0);
  REQUIRE(rc == SQLITE_OK);

  SECTION("Check the ChemicaLite version")
  {
    sqlite3_stmt *pStmt;
    rc = sqlite3_prepare_v2(db, "SELECT chemicalite_version()", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    rc = sqlite3_step(pStmt);
    REQUIRE(rc == SQLITE_ROW);

    std::string version = (const char *) sqlite3_column_text(pStmt, 0);
    REQUIRE(version == XSTRINGIFY(CHEMICALITE_VERSION));

    sqlite3_finalize(pStmt);
  }

  SECTION("Check the RDKit version")
  {
    sqlite3_stmt *pStmt;
    rc = sqlite3_prepare_v2(db, "SELECT rdkit_version()", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    rc = sqlite3_step(pStmt);
    REQUIRE(rc == SQLITE_ROW);

    std::string version = (const char *) sqlite3_column_text(pStmt, 0);
    REQUIRE(version == RDKit::rdkitVersion);

    sqlite3_finalize(pStmt);
  }

  SECTION("Check the RDKit build info")
  {
    sqlite3_stmt *pStmt;
    rc = sqlite3_prepare_v2(db, "SELECT rdkit_build()", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    rc = sqlite3_step(pStmt);
    REQUIRE(rc == SQLITE_ROW);

    std::string version = (const char *) sqlite3_column_text(pStmt, 0);
    REQUIRE(version == RDKit::rdkitBuild);

    sqlite3_finalize(pStmt);
  }

  SECTION("Check the Boost version")
  {
    sqlite3_stmt *pStmt;
    rc = sqlite3_prepare_v2(db, "SELECT boost_version()", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    rc = sqlite3_step(pStmt);
    REQUIRE(rc == SQLITE_ROW);

    std::string version = (const char *) sqlite3_column_text(pStmt, 0);
    REQUIRE(version == RDKit::boostVersion);

    sqlite3_finalize(pStmt);
  }

  // Close the db
  rc = sqlite3_close(db);
  REQUIRE(rc == SQLITE_OK);
}
