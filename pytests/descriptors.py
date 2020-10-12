#!/bin/env python
import unittest
import sys

from chemicalite import ChemicaLiteTestCase

class TestDescriptors(ChemicaLiteTestCase):

    def test_mw(self):
        c = self.db.cursor()
        c.execute("select mol_mw('C')")
        d = c.fetchone()[0]
        self.assertAlmostEqual(d, 16.043, places=4)
        c.execute("select mol_mw('CO')")
        d = c.fetchone()[0]
        self.assertAlmostEqual(d, 32.042, places=4)
        
    def test_logp(self):
        c = self.db.cursor()
        c.execute("select mol_logp('c1ccccc1')")
        d = c.fetchone()[0]
        self.assertAlmostEqual(d, 1.6866, places=4)
        c.execute("select mol_logp('c1ccccc1O')")
        d = c.fetchone()[0]
        self.assertAlmostEqual(d, 1.3922, places=4)
        c.execute("select mol_logp('CC(=O)O')")
        d = c.fetchone()[0]
        self.assertAlmostEqual(d, 0.0908999, places=4)

    def test_tpsa(self):
        c = self.db.cursor()
        c.execute("select mol_tpsa('c1ccccc1')")
        d = c.fetchone()[0]
        self.assertAlmostEqual(d, 0.0, places=4)
        c.execute("select mol_tpsa('CC(=O)O')")
        d = c.fetchone()[0]
        self.assertAlmostEqual(d, 37.3, places=4)

    def test_num_atms(self):
        c = self.db.cursor()
        c.execute("select mol_num_atms('c1ccccc1')")
        d = c.fetchone()[0]
        self.assertEqual(d, 12)
        c.execute("select mol_num_atms('CC(=O)O')")
        d = c.fetchone()[0]
        self.assertEqual(d, 8)

    def test_num_rings(self):
        c = self.db.cursor()
        c.execute("select mol_num_rings('c1ccccc1')")
        d = c.fetchone()[0]
        self.assertEqual(d, 1)
        c.execute("select mol_num_rings('CC(=O)O')")
        d = c.fetchone()[0]
        self.assertEqual(d, 0)

if __name__=="__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestDescriptors)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    errors = len(result.errors)
    failures = len(result.failures)
    sys.exit(errors + failures)

