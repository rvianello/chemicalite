#!/bin/env python

import unittest
import sys
import random

from chemicalite import ChemicaLiteTestCase

class TestRDtree(ChemicaLiteTestCase):

    def test_rdtree(self):
        self._create_vtab()

        c = self.db.cursor()
        
        d = next(c.execute("select count(*) from xyz"))[0]
        self.assertEqual(d, 0)

        self._fill_tree()
        
        d = next(c.execute("select count(*) from xyz"))[0]
        self.assertEqual(d, 256)

        for value in range(256):
            self._count_subset_matches(value)

    def test_github_00003(self):

        self._create_vtab()

        c = self.db.cursor()

        # insert a first bfp (any bfp would do, but the original github
        # ticket used an empty one)
        c.execute(
            "insert into xyz(id, signature) values(1, bfp_dummy(128, 0))")

        # and then update the record with a different bfp
        c.execute(
            "update xyz set signature=bfp_dummy(128, 42) where id=1")

    def test_github_00003_bis(self):

        self._create_vtab()

        c = self.db.cursor()

        # insert a first bfp (any bfp would do, but the original github
        # ticket used an empty one)
        c.execute(
            "insert into xyz(id, signature) values(1, bfp_dummy(128, 0))")

        # insert a second one, and show that update then works
        c.execute(
            "insert into xyz(id, signature) values(2, bfp_dummy(128, 33))")

        # and then update the record with a different bfp
        c.execute(
            "update xyz set signature=bfp_dummy(128, 42) where id=1")

    def test_github_00003_tris(self):

        self._create_vtab()

        c = self.db.cursor()

        # insert a first bfp (any bfp would do, but the original github
        # ticket used an empty one)
        c.execute(
            "insert into xyz(id, signature) values(1, bfp_dummy(128, 0))")

        # insert a second one, to see if it really works
        c.execute(
            "insert into xyz(id, signature) values(2, bfp_dummy(128, 33))")

        # but now update the second (and last record)
        c.execute(
            "update xyz set signature=bfp_dummy(128, 42) where id=2")

    def _create_vtab(self):
        c = self.db.cursor()
        c.execute(
            "create virtual table xyz "
            "using rdtree(id integer primary key, signature bytes(128))")

    def _fill_tree(self):
        c = self.db.cursor()
        values = list(range(256))
        random.shuffle(values)
        for v in values:
            c.execute(
                "insert into xyz(id, signature) "
                "values(?, bfp_dummy(128, ?))", (v, v))

    def _count_subset_matches(self, value):
        print("Testing resultset count for value ", value)
        expected = sum(x & value == value for x in range(256))
        print("Expected number of matches: ", expected)
        c = self.db.cursor()
        c.execute(
            "select count(*) from xyz where "
            "id match rdtree_subset(bfp_dummy(128, ?))", (value,))
        d = c.fetchone()[0]
        print("Found: ", d)
        self.assertEqual(d, expected)

if __name__=="__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestRDtree)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    errors = len(result.errors)
    failures = len(result.failures)
    sys.exit(errors + failures)

