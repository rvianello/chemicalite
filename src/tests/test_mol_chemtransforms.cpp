#include "test_common.hpp"

TEST_CASE("mol chemtransforms", "[mol]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  SECTION("test delete substructs")
  {
    test_select_value(
        db,
        "SELECT mol_to_smiles(mol_delete_substructs("
        "mol_from_smiles('CC(=O)O'), "
        "mol_from_smarts('C(=O)[OH]')"
        "))", "C");
  }

  SECTION("test replace substructs")
  {
    int rc = SQLITE_OK;

    sqlite3_stmt *pStmt = nullptr;
    rc = sqlite3_prepare(
        db,
        "SELECT mol_to_smiles(result) FROM "
        "mol_replace_substructs("
        "  mol_from_smiles('CC(=O)N'), "
        "  mol_from_smarts('[$(NC(=O))]'), "
        "  mol_from_smiles('OC')"
        ")",
        -1,
        &pStmt,
        0);

    REQUIRE(rc == SQLITE_OK);

    rc = sqlite3_step(pStmt);
    REQUIRE(rc == SQLITE_ROW);

    int value_type = sqlite3_column_type(pStmt, 0);
    REQUIRE(value_type == SQLITE_TEXT);

    std::string value { reinterpret_cast<const char *>(sqlite3_column_text(pStmt, 0)) };
    REQUIRE(value == "COC(C)=O");

    rc = sqlite3_step(pStmt);
    REQUIRE(rc == SQLITE_DONE);

    sqlite3_finalize(pStmt);
  }

  SECTION("test replace sidechains")
  {
    test_select_value(
        db,
        "SELECT mol_to_smiles(mol_replace_sidechains("
        "mol_from_smiles('BrCCc1cncnc1C(=O)O'), "
        "mol_from_smiles('c1cncnc1')"
        "))", "[1*]c1cncnc1[2*]");
  }

  SECTION("test replace core")
  {
    test_select_value(
        db,
        "SELECT mol_to_smiles(mol_replace_core("
        "mol_from_smiles('BrCCc1cncnc1C(=O)O'), "
        "mol_from_smiles('c1cncnc1')"
        "))", "[1*]CCBr.[2*]C(=O)O");
  }

  SECTION("test murcko decompose")
  {
    test_select_value(
        db,
        "SELECT mol_to_smiles(mol_murcko_decompose("
        "mol_from_smiles('c1ccc(=O)ccc1CC2CC2CCC')"
        "))", "O=c1cccc(CC2CC2)cc1");
  }

  test_db_close(db);
}

