#!/bin/env python
import sqlite3
import sys
import unittest

from chemicalite import ChemicaLiteTestCase

class TestBitString(ChemicaLiteTestCase):

    def test_layered(self):
        c = self.db.cursor()

        c.execute("select mol_layered_bfp('Cc1ccccc1')")
        self.assertIsInstance(c.fetchone()[0], bytes)

        c.execute("select mol_layered_bfp('BAD')")
        self.assertIsNone(c.fetchone()[0])

        c.execute(
            "select bfp_tanimoto("
            "mol_layered_bfp('Nc1ccccc1COC'), "
            "mol_layered_bfp('Nc1ccccc1COC')"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)

        c.execute(
            "select bfp_dice("
            "mol_layered_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
            "mol_layered_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)

    def test_rdkit(self):
        c = self.db.cursor()

        c.execute("select mol_rdkit_bfp('Cc1ccccc1')")
        self.assertIsInstance(c.fetchone()[0], bytes)

        c.execute("select mol_rdkit_bfp('BAD')")
        self.assertIsNone(c.fetchone()[0])

        c.execute(
            "select bfp_tanimoto("
            "mol_rdkit_bfp('Nc1ccccc1COC'), "
            "mol_rdkit_bfp('Nc1ccccc1COC')"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)

        c.execute(
            "select bfp_dice("
            "mol_rdkit_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
            "mol_rdkit_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)

    def test_atom_pairs(self):
        c = self.db.cursor()

        c.execute("select mol_atom_pairs_bfp('Cc1ccccc1')")
        self.assertIsInstance(c.fetchone()[0], bytes)

        c.execute("select mol_atom_pairs_bfp('BAD')")
        self.assertIsNone(c.fetchone()[0])

        c.execute(
            "select bfp_tanimoto("
            "mol_atom_pairs_bfp('Nc1ccccc1COC'), "
            "mol_atom_pairs_bfp('Nc1ccccc1COC')"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)

        c.execute(
            "select bfp_dice("
            "mol_atom_pairs_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
            "mol_atom_pairs_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)

    def test_topological_torsion(self):
        c = self.db.cursor()

        c.execute("select mol_topological_torsion_bfp('Cc1ccccc1')")
        self.assertIsInstance(c.fetchone()[0], bytes)

        c.execute("select mol_topological_torsion_bfp('BAD')")
        self.assertIsNone(c.fetchone()[0])

        c.execute(
            "select bfp_tanimoto("
            "mol_topological_torsion_bfp('Nc1ccccc1COC'), "
            "mol_topological_torsion_bfp('Nc1ccccc1COC')"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)

        c.execute(
            "select bfp_dice("
            "mol_topological_torsion_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
            "mol_topological_torsion_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)

    def test_maccs(self):
        c = self.db.cursor()

        c.execute("select mol_maccs_bfp('Cc1ccccc1')")
        self.assertIsInstance(c.fetchone()[0], bytes)

        c.execute("select mol_maccs_bfp('BAD')")
        self.assertIsNone(c.fetchone()[0])

        c.execute(
            "select bfp_tanimoto("
            "mol_maccs_bfp('Nc1ccccc1COC'), "
            "mol_maccs_bfp('Nc1ccccc1COC')"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)

        c.execute(
            "select bfp_dice("
            "mol_maccs_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
            "mol_maccs_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)

    def test_morgan(self):
        c = self.db.cursor()

        c.execute("select mol_morgan_bfp('Cc1ccccc1', 6)")
        self.assertIsInstance(c.fetchone()[0], bytes)

        c.execute("select mol_morgan_bfp('BAD', 6)")
        self.assertIsNone(c.fetchone()[0])

        c.execute(
            "select bfp_tanimoto("
            "mol_morgan_bfp('Nc1ccccc1COC', 6), "
            "mol_morgan_bfp('Nc1ccccc1COC', 6)"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)

        c.execute(
            "select bfp_dice("
            "mol_morgan_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12', 6), "
            "mol_morgan_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12', 6)"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)

    def test_feat_morgan(self):
        c = self.db.cursor()

        c.execute("select mol_feat_morgan_bfp('Cc1ccccc1', 6)")
        self.assertIsInstance(c.fetchone()[0], bytes)

        c.execute("select mol_feat_morgan_bfp('BAD', 6)")
        self.assertIsNone(c.fetchone()[0])

        c.execute(
            "select bfp_tanimoto("
            "mol_feat_morgan_bfp('Nc1ccccc1COC', 6), "
            "mol_feat_morgan_bfp('Nc1ccccc1COC', 6)"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)
        c.execute(
            "select bfp_dice("
            "mol_feat_morgan_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12', 6), "
            "mol_feat_morgan_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12', 6)"
            ")")
        d = c.fetchone()[0]
        self.assertEqual(d, 1.0)

    def test_mol_arg_mismatch(self):
        c = self.db.cursor()
        self.assertRaises(sqlite3.IntegrityError, lambda :
            c.execute(
                "select bfp_dice("
                "mol('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
                "mol_maccs_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
                ")"))
        self.assertRaises(sqlite3.IntegrityError, lambda :
            c.execute(
                "select bfp_tanimoto("
                "mol('Cn1cnc2n(C)c(=O)n(C)c(=O)c12'), "
                "mol_maccs_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
                ")"))

    def test_null_handling_in_similarity_functions(self):
        c = self.db.cursor()

        c.execute(
            "select bfp_tanimoto("
            "mol_rdkit_bfp('Nc1ccccc1COC'), "
            "NULL"
            ")")
        self.assertIsNone(c.fetchone()[0])

        c.execute(
            "select bfp_tanimoto("
            "NULL, "
            "mol_rdkit_bfp('Nc1ccccc1COC')"
            ")")
        self.assertIsNone(c.fetchone()[0])

        c.execute(
            "select bfp_dice("
            "mol_rdkit_bfp('Nc1ccccc1COC'), "
            "NULL"
            ")")
        self.assertIsNone(c.fetchone()[0])

        c.execute(
            "select bfp_dice("
            "NULL, "
            "mol_rdkit_bfp('Nc1ccccc1COC')"
            ")")
        self.assertIsNone(c.fetchone()[0])

    def test_bfp_weight(self):
        c = self.db.cursor()

        c.execute(
            "select bfp_weight("
            "mol_bfp_signature('BAD')"
            ")")
        self.assertIsNone(c.fetchone()[0])

        c.execute("select bfp_weight(NULL)")
        self.assertIsNone(c.fetchone()[0])

        c.execute(
            "select bfp_weight("
            "mol_bfp_signature('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
            ")")
        w = c.fetchone()[0]
        self.assertEqual(w, 355)

        c.execute(
            "select bfp_weight("
            "mol_maccs_bfp('Cn1cnc2n(C)c(=O)n(C)c(=O)c12')"
            ")")
        w = c.fetchone()[0]
        self.assertEqual(w, 46)


if __name__=="__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestBitString)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    errors = len(result.errors)
    failures = len(result.failures)
    sys.exit(errors + failures)