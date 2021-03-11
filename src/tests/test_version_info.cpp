#include "test_common.hpp"

#include <RDGeneral/versions.h>

#include "../utils.hpp"

TEST_CASE("ChemicaLite + RDKit version info", "[smoke]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  SECTION("Check the ChemicaLite version")
  {
    test_select_value(db, "SELECT chemicalite_version()", XSTRINGIFY(CHEMICALITE_VERSION));
  }

  SECTION("Check the RDKit version")
  {
    test_select_value(db, "SELECT rdkit_version()", RDKit::rdkitVersion);
  }

  SECTION("Check the RDKit build info")
  {
    test_select_value(db, "SELECT rdkit_build()", RDKit::rdkitBuild);
  }

  SECTION("Check the Boost version")
  {
    test_select_value(db, "SELECT boost_version()", RDKit::boostVersion);
  }

  test_db_close(db);
}
