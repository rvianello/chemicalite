#include "test_common.hpp"

TEST_CASE("bfp constructors + ops + descriptors", "[bfp]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  SECTION("test layered bfp")
  {
    test_select_value(
        db, 
        "SELECT bfp_tanimoto("
        "mol_layered_bfp(mol_from_smiles('Nc1ccccc1COC')), "
        "mol_layered_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 0.2877959927);
    test_select_value(
        db, 
        "SELECT bfp_dice("
        "mol_layered_bfp(mol_from_smiles('Nc1ccccc1COC')), "
        "mol_layered_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 0.4469589816);
  }

  SECTION("test rdkit bfp")
  {
    test_select_value(
        db, 
        "SELECT bfp_tanimoto("
        "mol_rdkit_bfp(mol_from_smiles('Nc1ccccc1COC')), "
        "mol_rdkit_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 0.1139240506);
    test_select_value(
        db, 
        "SELECT bfp_dice("
        "mol_rdkit_bfp(mol_from_smiles('Nc1ccccc1COC')), "
        "mol_rdkit_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 0.2045454545);
  }

  SECTION("test atom pairs bfp")
  {
    test_select_value(
        db, 
        "SELECT bfp_tanimoto("
        "mol_atom_pairs_bfp(mol_from_smiles('Nc1ccccc1COC')), "
        "mol_atom_pairs_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 0.1442307692);
    test_select_value(
        db, 
        "SELECT bfp_dice("
        "mol_atom_pairs_bfp(mol_from_smiles('Nc1ccccc1COC')), "
        "mol_atom_pairs_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 0.2521008403);
  }

  SECTION("test topological torsion bfp")
  {
    test_select_value(
        db, 
        "SELECT bfp_tanimoto("
        "mol_topological_torsion_bfp(mol_from_smiles('Nc1ccccc1COC')), "
        "mol_topological_torsion_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 0.);
    test_select_value(
        db, 
        "SELECT bfp_dice("
        "mol_topological_torsion_bfp(mol_from_smiles('Nc1ccccc1COC')), "
        "mol_topological_torsion_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 0.);
  }

  SECTION("test maccs bfp")
  {
    test_select_value(
        db, 
        "SELECT bfp_tanimoto("
        "mol_maccs_bfp(mol_from_smiles('Nc1ccccc1COC')), "
        "mol_maccs_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 0.2068965517);
    test_select_value(
        db, 
        "SELECT bfp_dice("
        "mol_maccs_bfp(mol_from_smiles('Nc1ccccc1COC')), "
        "mol_maccs_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 0.3428571429);
  }

  SECTION("test pattern bfp")
  {
    test_select_value(
        db, 
        "SELECT bfp_tanimoto("
        "mol_pattern_bfp(mol_from_smiles('Nc1ccccc1COC')), "
        "mol_pattern_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 0.2727272727);
    test_select_value(
        db, 
        "SELECT bfp_dice("
        "mol_pattern_bfp(mol_from_smiles('Nc1ccccc1COC')), "
        "mol_pattern_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 0.4285714286);
  }

  SECTION("test morgan bfp")
  {
    test_select_value(
        db, 
        "SELECT bfp_tanimoto("
        "mol_morgan_bfp(mol_from_smiles('Nc1ccccc1COC'), 2), "
        "mol_morgan_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), 2)"
        ")", 0.0975609756);
    test_select_value(
        db, 
        "SELECT bfp_dice("
        "mol_morgan_bfp(mol_from_smiles('Nc1ccccc1COC'), 2), "
        "mol_morgan_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), 2)"
        ")", 0.1777777778);
  }

  SECTION("test feat morgan bfp")
  {
    test_select_value(
        db, 
        "SELECT bfp_tanimoto("
        "mol_feat_morgan_bfp(mol_from_smiles('Nc1ccccc1COC'), 2), "
        "mol_feat_morgan_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), 2)"
        ")", 0.1176470588);
    test_select_value(
        db, 
        "SELECT bfp_dice("
        "mol_feat_morgan_bfp(mol_from_smiles('Nc1ccccc1COC'), 2), "
        "mol_feat_morgan_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), 2)"
        ")", 0.2105263158);
  }

  SECTION("test bfp weight")
  {
    test_select_value(
        db,
        "SELECT bfp_weight("
        "bfp_dummy(128, 3)"
        ")", 2*128/8);
    test_select_value(
        db,
        "SELECT bfp_weight("
        "mol_pattern_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 355);
    test_select_value(
        db,
        "SELECT bfp_weight("
        "mol_maccs_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 46);
  }

  SECTION("test bfp length")
  {
    test_select_value(
        db,
        "SELECT bfp_length("
        "bfp_dummy(128, 3)"
        ")", 128);
    test_select_value(
        db,
        "SELECT bfp_length("
        "mol_pattern_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'))"
        ")", 2048);
    test_select_value(
        db,
        "SELECT bfp_length("
        "mol_pattern_bfp(mol_from_smiles('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), 1024)"
        ")", 1024);
  }

  SECTION("test tanimoto similarity")
  {
    test_select_value(db, "SELECT bfp_tanimoto(bfp_dummy(128, 3), bfp_dummy(128, 0))", 0.0);
    test_select_value(db, "SELECT bfp_tanimoto(bfp_dummy(128, 3), bfp_dummy(128, 3))", 1.0);
    test_select_value(db, "SELECT bfp_tanimoto(bfp_dummy(128, 3), bfp_dummy(128, 1))", 0.5);
  }

  SECTION("test dice similarity")
  {
    test_select_value(db, "SELECT bfp_dice(bfp_dummy(128, 3), bfp_dummy(128, 0))", 0.0);
    test_select_value(db, "SELECT bfp_dice(bfp_dummy(128, 3), bfp_dummy(128, 3))", 1.0);
    test_select_value(db, "SELECT bfp_dice(bfp_dummy(128, 3), bfp_dummy(128, 1))", 0.6666666667);
  }

  test_db_close(db);
}
