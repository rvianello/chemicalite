#!/bin/env python
import sqlite3
import sys
import unittest

from chemicalite import ChemicaLiteTestCase

class TestMolecule(ChemicaLiteTestCase):

    def test_cast_to_mol(self):
        c = self.db.cursor()

        c.execute("select mol('CCC')")
        self.assertIsInstance(c.fetchone()[0], bytes)

        c.execute("select mol('C[*]C')")
        self.assertIsInstance(c.fetchone()[0], bytes)

        c.execute("select mol('')")
        # an empty SMILES is still a valid empty molecule
        self.assertIsInstance(c.fetchone()[0], bytes)

        c.execute("select mol('[C,N]')")
        self.assertIsNone(c.fetchone()[0])

        c.execute("select mol('BAD')")
        self.assertIsNone(c.fetchone()[0])

    def test_cast_to_qmol(self):
        c = self.db.cursor()

        c.execute("select qmol('CCC')")
        self.assertIsInstance(c.fetchone()[0], bytes)

        c.execute("select qmol('C[*]C')")
        self.assertIsInstance(c.fetchone()[0], bytes)

        c.execute("select qmol('[C,N]')")
        self.assertIsInstance(c.fetchone()[0], bytes)

        c.execute("select qmol('BAD')")
        self.assertIsNone(c.fetchone()[0])

    def test_mol_to_smiles(self):
        c = self.db.cursor()

        c.execute("select mol_smiles('C1=CC=CC=C1')")
        smiles = c.fetchone()[0]
        self.assertEqual(smiles, 'c1ccccc1')

        c.execute("select mol_smiles(NULL)")
        self.assertIsNone(c.fetchone()[0])

    def test_mol_is_substruct(self):
        c = self.db.cursor()
        c.execute("select mol_is_substruct('c1cccnc1C', 'c1cnccc1')")
        self.assertTrue(c.fetchone()[0])
        c.execute("select mol_is_substruct('c1ccccc1C', 'c1ncccc1')")
        self.assertFalse(c.fetchone()[0])

        c.execute("select mol_is_substruct('c1ccccc1', NULL)")
        self.assertIsNone(c.fetchone()[0])
        c.execute("select mol_is_substruct(NULL, 'c1ccccc1')")
        self.assertIsNone(c.fetchone()[0])

    def test_mol_is_superstruct(self):
        c = self.db.cursor()
        c.execute("select mol_is_superstruct('C1CCC1', 'C1CCC1C')")
        self.assertTrue(c.fetchone()[0])
        c.execute("select mol_is_superstruct('CCC', 'N')")
        self.assertFalse(c.fetchone()[0])

        c.execute("select mol_is_superstruct('c1ccccc1', NULL)")
        self.assertIsNone(c.fetchone()[0])
        c.execute("select mol_is_superstruct(NULL, 'c1ccccc1')")
        self.assertIsNone(c.fetchone()[0])

    def test_mol_cmp(self):
        c = self.db.cursor()
        c.execute("select mol_cmp('C1=CC=CC=C1', 'c1ccccc1')")
        self.assertTrue(c.fetchone()[0] == 0)
        c.execute("select mol_cmp('c1ccccc1N', 'c1cccnc1C')")
        self.assertFalse(c.fetchone()[0] == 0)

        c.execute("select mol_cmp('c1ccccc1', NULL)")
        self.assertIsNone(c.fetchone()[0])
        c.execute("select mol_cmp(NULL, 'c1ccccc1')")
        self.assertIsNone(c.fetchone()[0])
        
if __name__=="__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestMolecule)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    errors = len(result.errors)
    failures = len(result.failures)
    sys.exit(errors + failures)

