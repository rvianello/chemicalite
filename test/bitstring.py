#!/bin/env python
import unittest
import sys

from pysqlite2 import dbapi2 as sqlite3

from chemicalite import ChemicaLiteTestCase

class TestBitString(ChemicaLiteTestCase):

    def test_layered(self):
        d = self.db.execute(
            "select bfp_tanimoto("
            "mol_layered_bfp('Nc1ccccc1COC'), "
            "mol_layered_bfp('Nc1ccccc1COC')"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)
        d = self.db.execute(
            "select bfp_dice("
            "mol_layered_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
            "mol_layered_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)
        
    def test_rdkit(self):
        d = self.db.execute(
            "select bfp_tanimoto("
            "mol_rdkit_bfp('Nc1ccccc1COC'), "
            "mol_rdkit_bfp('Nc1ccccc1COC')"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)
        d = self.db.execute(
            "select bfp_dice("
            "mol_rdkit_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
            "mol_rdkit_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)
        
    def test_atom_pairs(self):
        d = self.db.execute(
            "select bfp_tanimoto("
            "mol_atom_pairs_bfp('Nc1ccccc1COC'), "
            "mol_atom_pairs_bfp('Nc1ccccc1COC')"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)
        d = self.db.execute(
            "select bfp_dice("
            "mol_atom_pairs_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
            "mol_atom_pairs_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)
        
    def test_topological_torsion(self):
        d = self.db.execute(
            "select bfp_tanimoto("
            "mol_topological_torsion_bfp('Nc1ccccc1COC'), "
            "mol_topological_torsion_bfp('Nc1ccccc1COC')"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)
        d = self.db.execute(
            "select bfp_dice("
            "mol_topological_torsion_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
            "mol_topological_torsion_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)
        
    def test_maccs(self):
        d = self.db.execute(
            "select bfp_tanimoto("
            "mol_maccs_bfp('Nc1ccccc1COC'), "
            "mol_maccs_bfp('Nc1ccccc1COC')"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)
        d = self.db.execute(
            "select bfp_dice("
            "mol_maccs_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
            "mol_maccs_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)
        
    def test_morgan(self):
        d = self.db.execute(
            "select bfp_tanimoto("
            "mol_morgan_bfp('Nc1ccccc1COC', 6), "
            "mol_morgan_bfp('Nc1ccccc1COC', 6)"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)
        d = self.db.execute(
            "select bfp_dice("
            "mol_morgan_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12', 6), "
            "mol_morgan_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12', 6)"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)
        
    def test_feat_morgan(self):
        d = self.db.execute(
            "select bfp_tanimoto("
            "mol_feat_morgan_bfp('Nc1ccccc1COC', 6), "
            "mol_feat_morgan_bfp('Nc1ccccc1COC', 6)"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)
        d = self.db.execute(
            "select bfp_dice("
            "mol_feat_morgan_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12', 6), "
            "mol_feat_morgan_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12', 6)"
            ")").fetchone()[0]
        self.assertEqual(d, 1.0)

    def test_mol_arg_mismatch(self):
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(
                "select bfp_dice("
                "mol('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
                "mol_maccs_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
                ")")
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(
                "select bfp_tanimoto("
                "mol('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
                "mol_maccs_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
                ")")


if __name__=="__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestBitString)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    errors = len(result.errors)
    failures = len(result.failures)
    sys.exit(errors + failures)

