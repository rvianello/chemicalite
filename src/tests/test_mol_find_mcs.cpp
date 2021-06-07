#include "test_common.hpp"

TEST_CASE("mol find MCS", "[mol]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  SECTION("test mol find MCS")
  {
    int rc = SQLITE_OK;

    rc = sqlite3_exec(
        db, 
        "CREATE TABLE mols(id INTEGER PRIMARY KEY, mol MOL);"
        "INSERT INTO mols(mol) VALUES (mol_from_smiles('c1ccccc1C'));"
        "INSERT INTO mols(mol) VALUES (mol_from_smiles('Cc1ccccc1C'));"
        "INSERT INTO mols(mol) VALUES (mol_from_smiles('c1ccccc1CC'));",
        NULL, NULL, NULL);
    REQUIRE(rc == SQLITE_OK);

    test_select_value(
      db,
      "SELECT mol_to_smarts(mol_find_mcs(mol)) FROM mols",
      "[#6]1:[#6]:[#6]:[#6]:[#6]:[#6]:1-[#6]");

  }

  test_db_close(db);
}
