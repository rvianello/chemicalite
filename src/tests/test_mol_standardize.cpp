#include "test_common.hpp"


TEST_CASE("mol standardizer", "[mol]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  SECTION("mol_cleanup")
  {
    test_select_value(db, "SELECT mol_to_smiles(mol_cleanup(mol_from_smiles('CCC(=O)O[Hg]')))", "CCC(=O)[O-].[Hg+]");
  }
  SECTION("mol_normalize")
  {
    test_select_value(db, "SELECT mol_to_smiles(mol_normalize(mol_from_smiles('CS(C)=O')))", "C[S+](C)[O-]");
  }
  SECTION("mol_reionize")
  {
    test_select_value(db, "SELECT mol_to_smiles(mol_reionize(mol_from_smiles('[Na].O=C(O)c1ccccc1')))", "O=C([O-])c1ccccc1.[Na+]");
  }
  SECTION("mol_remove_fragments")
  {
    test_select_value(db, "SELECT mol_to_smiles(mol_remove_fragments(mol_from_smiles('CN(C)C.Cl.Cl.Br')))", "CN(C)C");
  }
  SECTION("mol_canonical_tautomer")
  {
    test_select_value(db, "SELECT mol_to_smiles(mol_canonical_tautomer(mol_from_smiles('C1(=CCCCC1)O')))", "O=C1CCCCC1");
  }

  SECTION("mol_tautomer_parent")
  {
    test_select_value(db, "SELECT mol_to_smiles(mol_tautomer_parent(mol_from_smiles('C1(=CCCCC1)O')))", "O=C1CCCCC1");
  }
  SECTION("mol_fragment_parent")
  {
    test_select_value(db, "SELECT mol_to_smiles(mol_fragment_parent(mol_from_smiles('O=C(O)c1ccccc1.O=C(O)c1ccccc1.O=C(O)c1ccccc1')))", "O=C(O)c1ccccc1");
  }
  SECTION("mol_stereo_parent")
  {
    test_select_value(db, "SELECT mol_to_smiles(mol_stereo_parent(mol_from_smiles('C[C@](F)(Cl)C/C=C/[C@H](F)Cl')))", "CC(F)(Cl)CC=CC(F)Cl");
  }
  SECTION("mol_isotope_parent")
  {
    test_select_value(db, "SELECT mol_to_smiles(mol_isotope_parent(mol_from_smiles('[12CH3][13CH3]')))", "CC");
  }
  SECTION("mol_charge_parent")
  {
    test_select_value(db, "SELECT mol_to_smiles(mol_charge_parent(mol_from_smiles('C(C(=O)[O-])(Cc1n[n-]nn1)(C[NH3+])(C[N+](=O)[O-])')))", "NCC(Cc1nn[nH]n1)(C[N+](=O)[O-])C(=O)O");
  }
  SECTION("mol_super_parent")
  {
    test_select_value(db, "SELECT mol_to_smiles(mol_super_parent(mol_from_smiles('[O-]c1c([12C@H](F)Cl)c(O[2H])c(C(=O)O)cc1CC=CO.[Na+]')))", "O=CCCc1cc(C(=O)O)c(O)c(C(F)Cl)c1O");
  }

  test_db_close(db);
}
