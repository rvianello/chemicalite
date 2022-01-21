#include "test_common.hpp"

TEST_CASE("SMILES reader", "[smi reader]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  SECTION("vtab - cdk2")
  {
    int rc;
  
    rc = sqlite3_exec(
        db,
        "CREATE VIRTUAL TABLE cdk2 USING smi_reader("
        "'cdk2_stereo.csv', delimiter=',',title_line=0)",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    test_select_value(db, "SELECT COUNT(*) FROM cdk2", 2);
    test_select_value(db, "SELECT MAX(mol_amw(molecule)) FROM cdk2", 295.775);
  }

  SECTION("table valued function - cdk2")
  {
    test_select_value(
        db, 
        "SELECT COUNT(*) FROM smi_reader('cdk2_stereo.csv') "
        "WHERE delimiter=',' AND title_line=0",
        2);
    test_select_value(
        db, 
        "SELECT MAX(mol_amw(molecule)) FROM smi_reader('cdk2_stereo.csv') "
        "WHERE delimiter=',' AND title_line=0",
        295.775);
  }

  SECTION("vtab - chembl")
  {
    int rc;
  
    rc = sqlite3_exec(
        db,
        "CREATE VIRTUAL TABLE chembl USING smi_reader("
        "'chembl_29_sample.txt', smiles_column=1, name_column=0)",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    test_select_value(db, "SELECT COUNT(*) FROM chembl", 10);
    test_select_value(db, "SELECT MAX(mol_amw(molecule)) FROM chembl", 3548.213);
  }

  SECTION("table valued function - chembl")
  {
      test_select_value(
        db,
        "SELECT COUNT(*) FROM smi_reader("
        "'chembl_29_sample.txt') WHERE smiles_column=1 AND name_column=0",
        10);

      test_select_value(
        db,
        "SELECT MAX(mol_amw(molecule)) FROM smi_reader("
        "'chembl_29_sample.txt') WHERE smiles_column=1 AND name_column=0",
        3548.213);

  }

  SECTION("vtab - tpsa no title")
  {
    int rc;
  
    rc = sqlite3_exec(
        db,
        "CREATE VIRTUAL TABLE tpsa USING smi_reader("
        "'fewSmi.csv', delimiter=',', "
        "smiles_column=1, name_column=0, title_line=0)",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    test_select_value(db, "SELECT COUNT(*) FROM tpsa", 10);
    test_select_value(db, "SELECT MAX(mol_amw(molecule)) FROM tpsa", 490.103);
  }

  SECTION("table valued function - tpsa no title")
  {
    test_select_value(
        db,
        "SELECT COUNT(*) FROM smi_reader('fewSmi.csv') "
        "WHERE delimiter=',' AND "
        "smiles_column=1 AND name_column=0 AND title_line=0",
        10);

    test_select_value(
        db,
        "SELECT MAX(mol_amw(molecule)) FROM smi_reader('fewSmi.csv') "
        "WHERE delimiter=',' AND "
        "smiles_column=1 AND name_column=0 AND title_line=0",
        490.103);
  }

  SECTION("vtab - tpsa")
  {
    int rc;
  
    rc = sqlite3_exec(
        db,
        "CREATE VIRTUAL TABLE tpsa USING smi_reader("
        "'fewSmi.2.csv', delimiter=',', "
        "smiles_column=1, name_column=0, title_line=1)",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    test_select_value(db, "SELECT COUNT(*) FROM tpsa", 10);
    test_select_value(db, "SELECT MAX(mol_amw(molecule)) FROM tpsa", 490.103);
  }

  SECTION("table valued function - tpsa")
  {
    test_select_value(
        db,
        "SELECT COUNT(*) FROM smi_reader('fewSmi.2.csv') "
        "WHERE delimiter=',' AND smiles_column=1 AND name_column=0",
        10);

    test_select_value(
        db,
        "SELECT MAX(mol_amw(molecule)) FROM smi_reader('fewSmi.2.csv') "
        "WHERE delimiter=',' AND smiles_column=1 AND name_column=0",
        490.103);
  }

  SECTION("vtab w/ schema - tpsa")
  {
    int rc;
  
    rc = sqlite3_exec(
        db,
        "CREATE VIRTUAL TABLE tpsa USING smi_reader("
        "'fewSmi.2.csv', delimiter=',', "
        "smiles_column=1, name_column=0,"
        "schema='TPSA REAL')",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    test_select_value(db, "SELECT MAX(TPSA) FROM tpsa", 106.51);
  }

  test_db_close(db);
}