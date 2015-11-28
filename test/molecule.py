#!/bin/env python
import unittest
import sys

try:
    import apsw
except ImportError:
    pass

from chemicalite import ChemicaLiteTestCase

class TestMolecule(ChemicaLiteTestCase):

    def test_cast_to_mol(self):
        c = self.db.cursor()
        c.execute("select mol('CCC')")
        c.execute("select mol('C[*]C')")
        self.assertRaises(apsw.SQLError,lambda : c.execute("select mol('[C,N]')"))
        self.assertRaises(apsw.SQLError,lambda : c.execute("select mol('BAD')"))

    def test_cast_to_qmol(self):
        c = self.db.cursor()        
        c.execute("select qmol('CCC')")
        c.execute("select qmol('C[*]C')")
        c.execute("select qmol('[C,N]')")
        self.assertRaises(apsw.SQLError,lambda : c.execute("select qmol('BAD')"))

    def test_mol_to_smiles(self):
        c = self.db.cursor()
        c.execute("select mol_smiles('C1=CC=CC=C1')")
        smiles = next(c)[0]
        self.assertEqual(smiles, 'c1ccccc1')

    def test_mol_is_substruct(self):
        c = self.db.cursor()
        c.execute("select mol_is_substruct('c1cccnc1C', 'c1cnccc1')")
        self.assertTrue(next(c)[0])
        c.execute("select mol_is_substruct('c1ccccc1C', 'c1ncccc1')")
        self.assertFalse(next(c)[0])

    def test_mol_is_superstruct(self):
        c = self.db.cursor()
        c.execute("select mol_is_superstruct('C1CCC1', 'C1CCC1C')")
        self.assertTrue(next(c)[0])
        c.execute("select mol_is_superstruct('CCC', 'N')")
        self.assertFalse(next(c)[0])

    def test_mol_cmp(self):
        c = self.db.cursor()
        c.execute("select mol_cmp('C1=CC=CC=C1', 'c1ccccc1')")
        self.assertTrue(next(c)[0] == 0)
        c.execute("select mol_cmp('c1ccccc1N', 'c1cccnc1C')")
        self.assertFalse(next(c)[0] == 0)

        
if __name__=="__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestMolecule)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    errors = len(result.errors)
    failures = len(result.failures)
    sys.exit(errors + failures)

