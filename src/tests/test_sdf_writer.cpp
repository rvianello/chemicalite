#include "test_common.hpp"


TEST_CASE("SDF writer", "[sdf_writer]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  SECTION("plain copy")
  {
    test_select_value(
      db, 
      "SELECT sdf_writer(molecule, '/tmp/copy.sdf') FROM sdf_reader('cdk2.sdf')", 47);

    test_select_value(db, "SELECT COUNT(*) FROM sdf_reader('/tmp/copy.sdf')", 47);
  }

  SECTION("filtered copy")
  {
    test_select_value(
      db, 
      "SELECT sdf_writer(molecule, '/tmp/copy.sdf') FROM sdf_reader('cdk2.sdf')"
      " WHERE mol_amw(molecule) < 350.0", 23);

    test_select_value(db, "SELECT COUNT(*) FROM sdf_reader('/tmp/copy.sdf')", 23);
  }

  test_db_close(db);
}
