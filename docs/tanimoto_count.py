#!/bin/env python
import sys
import time

from pysqlite2 import dbapi2 as sqlite3

def search(c, target, threshold):
    t1 = time.time()
    count = c.execute("SELECT count(*) FROM "
                      "morgan as idx WHERE "
                      "idx.id match rdtree_tanimoto(mol_morgan_bfp(?, 2), ?)",
                      (target, threshold)).fetchone()[0]
    t2 = time.time()
    return count, t2-t1

def tanimoto_count(chemicalite_path, chembldb_sql, target, threshold):

    db = sqlite3.connect(chembldb_sql)
    db.enable_load_extension(True)
    db.load_extension(chemicalite_path)
    db.enable_load_extension(False)

    c = db.cursor()

    print 'Target structure: {0}'.format(target)
    print 'Minimum Tanimoto similarity: {0}'.format(threshold)

    count, t = search(c, target, float(threshold))

    print ('Found {0} matches in {1} seconds').format(count, t)

    db.close()

if __name__=="__main__":
    tanimoto_count(*sys.argv[1:5])
