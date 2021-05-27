#include "test_common.hpp"


TEST_CASE("SDF I/O", "[sdf_reader]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  SECTION("basic")
  {
    int rc = sqlite3_exec(
        db, 
        "CREATE VIRTUAL TABLE cdk2 USING sdf_reader('cdk2.sdf')",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    test_select_value(db, "SELECT COUNT(*) FROM cdk2", 47);

    test_select_value(db, "SELECT MAX(mol_amw(molecule)) FROM cdk2", 449.517);
  }

  SECTION("with props")
  {
    int rc = sqlite3_exec(
        db, 
        "CREATE VIRTUAL TABLE cdk2 USING sdf_reader("
        "'cdk2.sdf', "
        "schema='_Name TEXT AS name, \"r_mmffld_Potential_Energy-OPLS_2005\" REAL AS energy'"
        ")",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    test_select_value(
      db, "SELECT name FROM cdk2 WHERE energy = (SELECT MIN(energy) FROM cdk2)", "ZINC03814453");
    test_select_value(
      db, "SELECT mol_to_smiles(molecule) FROM cdk2 WHERE energy = (SELECT MAX(energy) FROM cdk2)",
      "Cc1nc(C)c(-c2[nH]nc3c2C(=O)c2c(NC(=O)NN4CC[NH+](C)CC4)cccc2-3)s1");
  }

  test_db_close(db);
}
