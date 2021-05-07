#!/bin/env python
from __future__ import print_function

import sys
import time

import apsw

def search(c, target, threshold):
    t1 = time.time()
    count = c.execute("SELECT count(*) FROM "
                      "morgan as idx WHERE "
                      "idx.id match rdtree_tanimoto(mol_morgan_bfp(?, 2), ?)",
                      (target, threshold)).fetchall()[0][0]
    t2 = time.time()
    return count, t2-t1

def tanimoto_count(chemicalite_path, chembldb_sql, target, threshold):

    connection = apsw.Connection(chembldb_sql)
    connection.enableloadextension(True)
    connection.loadextension(chemicalite_path)
    connection.enableloadextension(False)

    cursor = connection.cursor()

    print('Target structure:', target)
    print('Minimum Tanimoto similarity:', threshold)

    count, t = search(cursor, target, float(threshold))

    print('Found {0} matches in {1} seconds'.format(count, t))


if __name__=="__main__":
    tanimoto_count(*sys.argv[1:5])
