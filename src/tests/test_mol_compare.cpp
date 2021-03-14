#include "test_common.hpp"

TEST_CASE("mol compare", "[mol]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  SECTION("test mol_compare")
  {
    test_select_value(
        db,
        "SELECT mol_cmp("
        "mol_from_smiles('c1ccccc1C'), "
        "mol_from_smiles('c1ccccc1')"
        ")", 1);
    test_select_value(
        db,
        "SELECT mol_cmp("
        "mol_from_smiles('c1ccccc1C'), "
        "mol_from_smiles('c1ccccc1C')"
        ")", 0);
    test_select_value(
        db,
        "SELECT mol_cmp("
        "mol_from_smiles('c1ccccc1'), "
        "mol_from_smiles('c1ccccc1C')"
        ")", -1);
  }

  SECTION("test mol_is_substruct")
  {
    test_select_value(
        db,
        "SELECT mol_is_substruct("
        "mol_from_smiles('c1ccccc1C'), "
        "mol_from_smiles('c1ccccc1')"
        ")", 1);
    test_select_value(
        db,
        "SELECT mol_is_substruct("
        "mol_from_smiles('c1ccccc1C'), "
        "mol_from_smiles('c1ccccc1C')"
        ")", 1);
    test_select_value(
        db,
        "SELECT mol_is_substruct("
        "mol_from_smiles('c1ccccc1'), "
        "mol_from_smiles('c1ccccc1C')"
        ")", 0);
  }

  SECTION("test mol_is_superstruct")
  {
    test_select_value(
        db,
        "SELECT mol_is_superstruct("
        "mol_from_smiles('c1ccccc1C'), "
        "mol_from_smiles('c1ccccc1')"
        ")", 0);
    test_select_value(
        db,
        "SELECT mol_is_superstruct("
        "mol_from_smiles('c1ccccc1C'), "
        "mol_from_smiles('c1ccccc1C')"
        ")", 1);
    test_select_value(
        db,
        "SELECT mol_is_superstruct("
        "mol_from_smiles('c1ccccc1'), "
        "mol_from_smiles('c1ccccc1C')"
        ")", 1);
  }

  SECTION("test smarts substruct")
  {
    test_select_value(
        db,
        "SELECT mol_is_substruct("
        "mol_from_smiles('c1ccccc1'), "
        "mol_from_smarts('c1cc[c,n]cc1')"
        ")", 1);
    test_select_value(
        db,
        "SELECT mol_is_substruct("
        "mol_from_smiles('c1ccccn1'), "
        "mol_from_smarts('c1cc[c,n]cc1')"
        ")", 1);
  }

  test_db_close(db);
}
