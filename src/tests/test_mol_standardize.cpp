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

    test_select_value(
      db,
      R"SQL(
        SELECT 
          mol_to_smiles(
            mol_normalize(
              mol_from_smiles('ClCCCBr'),
              '{"normalizationData":[
                   {"name":"silly 1","smarts":"[Cl:1]>>[F:1]"},
                   {"name":"silly 2","smarts":"[Br:1]>>[F:1]"}
                ]}'
            )
          )
      )SQL",
      "FCCCF"
    );
  }
  SECTION("mol_reionize")
  {
    test_select_value(db, "SELECT mol_to_smiles(mol_reionize(mol_from_smiles('[Na].O=C(O)c1ccccc1')))", "O=C([O-])c1ccccc1.[Na+]");

    test_select_value(
      db,
      R"SQL(
        SELECT 
          mol_to_smiles(
            mol_reionize(
              mol_from_smiles('c1cc([O-])cc(C(=O)O)c1'),
              '{"acidbaseData":[{"name":"-CO2H","acid":"C(=O)[OH]","base":"C(=O)[O-]"},{"name":"phenol","acid":"c[OH]","base":"c[O-]"}]}'
            )
          )
      )SQL",
      "O=C([O-])c1cccc(O)c1"
    );

    test_select_value(
      db,
      R"SQL(
        SELECT 
          mol_to_smiles(
            mol_reionize(
              mol_from_smiles('C1=C(C=CC(=C1)[S]([O-])=O)[S](O)(=O)=O'),
              '{"acidbaseData":[{"name":"-CO2H","acid":"C(=O)[OH]","base":"C(=O)[O-]"},{"name":"phenol","acid":"c[OH]","base":"c[O-]"}]}'
            )
          )
      )SQL",
      "O=S([O-])c1ccc(S(=O)(=O)O)cc1"
    );

  }
  SECTION("mol_remove_fragments")
  {
    test_select_value(db, "SELECT mol_to_smiles(mol_remove_fragments(mol_from_smiles('CN(C)C.Cl.Cl.Br')))", "CN(C)C");

    test_select_value(db, "SELECT mol_to_smiles(mol_remove_fragments(mol_from_smiles('[F-].[Cl-].[Br-].CC')))", "CC");

    test_select_value(
      db,
      R"SQL(
        SELECT 
          mol_to_smiles(
            mol_remove_fragments(
              mol_from_smiles('[F-].[Cl-].[Br-].CC'),
              '{"fragmentData":[
                   {"name":"hydrogen", "smarts":"[H]"}, 
                   {"name":"fluorine", "smarts":"[F]"}, 
                   {"name":"chlorine", "smarts":"[Cl]"}
              ]}'
            )
          )
      )SQL",
      "CC.[Br-]"
    );

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
