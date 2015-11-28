#!/bin/env python
import sys
import unittest

try:
    import apsw
except ImportError:
    apsw = None

class ChemicaLiteTestCase(unittest.TestCase):

    def setUp(self):
        if apsw is None:
            raise AssertionError('Python tests require the APWS driver')
        self.db = apsw.Connection(':memory:')
        self.db.enableloadextension(True)
        self.db.loadextension('chemicalite')
        self.db.enableloadextension(False)
