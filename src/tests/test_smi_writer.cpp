#include "test_common.hpp"

TEST_CASE("SMILES writer", "[smi_writer]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  SECTION("plain copy")
  {
    test_select_value(
      db, 
      "SELECT smi_writer(molecule, '/tmp/copy.smi') "
      "FROM smi_reader('chembl_29_sample.txt') WHERE smiles_column=1 AND name_column=0", 10);

    test_select_value(db, "SELECT COUNT(*) FROM smi_reader('/tmp/copy.smi')", 10);
  }

  SECTION("3 args")
  {
    test_select_value(
      db, 
      "SELECT smi_writer(molecule, '/tmp/copy.smi', ';') "
      "FROM smi_reader('chembl_29_sample.txt') WHERE smiles_column=1 AND name_column=0", 10);

    test_select_value(db, "SELECT COUNT(*) FROM smi_reader('/tmp/copy.smi') WHERE delimiter=';'", 10);
  }

  SECTION("4 args")
  {
    test_select_value(
      db, 
      "SELECT smi_writer(molecule, '/tmp/copy.smi', NULL, 'ID') "
      "FROM smi_reader('chembl_29_sample.txt') WHERE smiles_column=1 AND name_column=0", 10);

    test_select_value(db, "SELECT COUNT(*) FROM smi_reader('/tmp/copy.smi')", 10);
  }

  SECTION("5 args")
  {
    test_select_value(
      db, 
      "SELECT smi_writer(molecule, '/tmp/copy.smi', NULL, NULL, 0) "
      "FROM smi_reader('chembl_29_sample.txt') WHERE smiles_column=1 AND name_column=0", 10);

    test_select_value(db, "SELECT COUNT(*) FROM smi_reader('/tmp/copy.smi') WHERE title_line=0", 10);
  }

  SECTION("6 args")
  {
    test_select_value(
      db, 
      "SELECT smi_writer(molecule, '/tmp/copy.smi', ';', NULL, 0, 0) "
      "FROM smi_reader('chembl_29_sample.txt') WHERE smiles_column=1 AND name_column=0", 10);

    test_select_value(
      db,
      "SELECT COUNT(*) FROM smi_reader('/tmp/copy.smi') "
      "WHERE delimiter=';' AND title_line=0", 10);
  }

  SECTION("filtered copy")
  {
     test_select_value(
      db, 
      "SELECT smi_writer(molecule, '/tmp/copy.smi') "
      "FROM smi_reader('chembl_29_sample.txt') "
      "WHERE smiles_column=1 AND name_column=0 AND mol_amw(molecule) < 500.0", 4);

    test_select_value(db, "SELECT COUNT(*) FROM smi_reader('/tmp/copy.smi')", 4);
  }

  test_db_close(db);
}