#!/bin/env python
import sys
import unittest

from pysqlite2 import dbapi2 as sqlite3

class ChemicaLiteTestCase(unittest.TestCase):

    def setUp(self):
        self.extension_path = sys.argv[1]
        self.db = sqlite3.connect(':memory:')
        self.db.enable_load_extension(True)
        self.db.load_extension(self.extension_path)
        self.db.enable_load_extension(False)

