#include "test_common.hpp"


TEST_CASE("mol descriptors", "[mol]")
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

  SECTION("mol_amw")
  {
    test_select_value(db, "SELECT mol_amw(mol_from_smiles('C'))", 16.043);
    test_select_value(db, "SELECT mol_amw(mol_from_smiles('CO'))", 32.042);
  }

  SECTION("mol_tpsa")
  {
    test_select_value(db, "SELECT mol_amw(mol_from_smiles('CCO'))", 46.069);
    test_select_value(db, "SELECT mol_amw(mol_from_smiles('c1ccccn1'))", 79.102);
  }

  SECTION("mol_fraction_csp3")
  {
    test_select_value(db, "SELECT mol_fraction_csp3(mol_from_smiles('C=CC'))", 1./3);
    test_select_value(db, "SELECT mol_fraction_csp3(mol_from_smiles('c1ccccn1'))", 0.);
  }

  SECTION("mol_hba")
  {
    test_select_value(db, "SELECT mol_hba(mol_from_smiles('Oc1ccccc1C=O'))", 2);
  }

  SECTION("mol_hbd")
  {
    test_select_value(db, "SELECT mol_hbd(mol_from_smiles('Oc1ccccc1C=O'))", 1);
  }

  SECTION("mol_num_rotatable_bonds")
  {
    test_select_value(db, "SELECT mol_num_rotatable_bonds(mol_from_smiles('NCCCN'))", 2);
  }

  SECTION("mol_num_hetatms")
  {
    test_select_value(db, "SELECT mol_num_hetatms(mol_from_smiles('Oc1ccccn1'))", 2);
  }

  SECTION("mol_num_rings")
  {
    test_select_value(db, "SELECT mol_num_rings(mol_from_smiles('Oc1ccccn1'))", 1);
    test_select_value(db, "SELECT mol_num_rings(mol_from_smiles('OCCCCN'))", 0);
  }

  SECTION("mol_num_aromatic_rings")
  {
    test_select_value(db, "SELECT mol_num_aromatic_rings(mol_from_smiles('Oc1ccccn1'))", 1);
    test_select_value(db, "SELECT mol_num_aromatic_rings(mol_from_smiles('OC1CCCCN1'))", 0);
  }

  SECTION("mol_num_aliphatic_rings")
  {
    test_select_value(db, "SELECT mol_num_aliphatic_rings(mol_from_smiles('Oc1ccccn1'))", 0);
    test_select_value(db, "SELECT mol_num_aliphatic_rings(mol_from_smiles('OC1CCCCN1'))", 1);
  }

  SECTION("mol_num_saturated_rings")
  {
    test_select_value(db, "SELECT mol_num_saturated_rings(mol_from_smiles('Oc1ccccn1'))", 0);
    test_select_value(db, "SELECT mol_num_saturated_rings(mol_from_smiles('OC1CC=CCN1'))", 0);
    test_select_value(db, "SELECT mol_num_saturated_rings(mol_from_smiles('OC1CCCCN1'))", 1);
  }

  // TODO add tests for the missing descriptors

  SECTION("mol_logp")
  {
    test_select_value(db, "SELECT mol_logp(mol_from_smiles('C=CC(=O)O'))", 0.257);
    test_select_value(db, "SELECT mol_logp(mol_from_smiles('c1ccccn1'))", 1.0816);
  }

  SECTION("mol_num_atms")
  {
    test_select_value(db, "SELECT mol_num_atms(mol_from_smiles('C=CC'))", 9);
    test_select_value(db, "SELECT mol_num_atms(mol_from_smiles('c1ccccn1'))", 11);
  }

  SECTION("mol_num_hvyatms")
  {
    test_select_value(db, "SELECT mol_num_hvyatms(mol_from_smiles('C=CC'))", 3);
    test_select_value(db, "SELECT mol_num_hvyatms(mol_from_smiles('c1ccccn1'))", 6);
  }

  SECTION("mol_formula")
  {
    test_select_value(db, "SELECT mol_formula(mol_from_smiles('NC1CC=CCN1'))", "C5H10N2");
    test_select_value(db, "SELECT mol_formula(mol_from_smiles('OC1CCCCN1'))", "C5H11NO");
  }

  // Close the db
  rc = sqlite3_close(db);
  REQUIRE(rc == SQLITE_OK);
}
