#!/bin/env python
import unittest
import sys
import random

try:
    from pysqlite2 import dbapi2 as sqlite3
except ImportError:
    import sqlite3

from chemicalite import ChemicaLiteTestCase

class TestRDtree(ChemicaLiteTestCase):

    def test_rdtree(self):
        self._create_vtab()
        
        d = self.db.execute("select count(*) from xyz").fetchone()[0]
        self.assertEqual(d, 0)

        self._fill_tree()
        
        d = self.db.execute("select count(*) from xyz").fetchone()[0]
        self.assertEqual(d, 256)

        for value in range(256):
            self._count_subset_matches(value)

    def _create_vtab(self):
        self.db.execute(
            "create virtual table xyz "
            "using rdtree(id integer primary key, signature bytes(128))")

    def _fill_tree(self):
        values = range(256)
        random.shuffle(values)
        for v in values:
            self.db.execute(
                "insert into xyz(id, signature) "
                "values(?, bfp_dummy(128, ?))", (v, v))

    def _count_subset_matches(self, value):
        print "Testing resultset count for value {0}".format(value)
        expected = sum(x & value == value for x in range(256))
        print "Expected number of matches: {0}".format(expected)
        d = self.db.execute(
            "select count(*) from xyz where "
            "id match rdtree_subset(bfp_dummy(128, ?))", (value,)).fetchone()[0]
        print "Found: {0}".format(d)
        self.assertEqual(d, expected)

if __name__=="__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestRDtree)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    errors = len(result.errors)
    failures = len(result.failures)
    sys.exit(errors + failures)

