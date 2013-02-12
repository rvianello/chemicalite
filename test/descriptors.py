#!/bin/env python
import unittest
import sys

from pysqlite2 import dbapi2 as sqlite3

from chemicalite import ChemicaLiteTestCase

class TestDescriptors(ChemicaLiteTestCase):

    def test_descriptor_mw(self):
        d = self.db.execute("select mol_mw('C')").fetchone()[0]
        self.assertAlmostEqual(d, 16.043, delta=1e-7)
        d = self.db.execute("select mol_mw('CO')").fetchone()[0]
        self.assertAlmostEqual(d, 32.042, delta=1e-7)
        
    def test_descriptor_logp(self):
        d = self.db.execute("select mol_logp('c1ccccc1')").fetchone()[0]
        self.assertAlmostEqual(d, 1.6866, delta=1e-7)
        d = self.db.execute("select mol_logp('c1ccccc1O')").fetchone()[0]
        self.assertAlmostEqual(d, 1.3922, delta=1e-7)
        d = self.db.execute("select mol_logp('CC(=O)O')").fetchone()[0]
        self.assertAlmostEqual(d, 0.0908999, delta=1e-7)

        
if __name__=="__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestDescriptors)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    errors = len(result.errors)
    failures = len(result.failures)
    sys.exit(errors + failures)

