#!/bin/env python
import sqlite3
import sys
import unittest

class ChemicaLiteTestCase(unittest.TestCase):

    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.enable_load_extension(True)
        self.db.load_extension('chemicalite')
        self.db.enable_load_extension(False)
