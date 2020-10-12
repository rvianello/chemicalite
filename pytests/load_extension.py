#!/bin/env python
import unittest
import sys

from chemicalite import ChemicaLiteTestCase

class TestLoadExtension(ChemicaLiteTestCase):

    def test_load_extension(self):
        pass

if __name__=="__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestLoadExtension)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    errors = len(result.errors)
    failures = len(result.failures)
    sys.exit(errors + failures)

