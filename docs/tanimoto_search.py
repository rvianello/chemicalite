#!/bin/env python
import sys
import time

from pysqlite2 import dbapi2 as sqlite3

def search(c, target, threshold):
    t1 = time.time()
    rs = c.execute(
        "SELECT c.chembl_id, c.smiles, "
        "bfp_tanimoto(mol_morgan_bfp(c.molecule, 2), mol_morgan_bfp(?, 2)) as t "
        "FROM "
        "chembl as c JOIN "
        "(SELECT id FROM morgan WHERE "
        "id match rdtree_tanimoto(mol_morgan_bfp(?, 2), ?)) as idx "
        "USING(id) ORDER BY t DESC",
        (target, target, threshold)).fetchall()
    t2 = time.time()
    return rs, t2-t1

def tanimoto_search(chemicalite_path, chembldb_sql, target, threshold):
    db = sqlite3.connect(chembldb_sql)
    db.enable_load_extension(True)
    db.load_extension(chemicalite_path)
    db.enable_load_extension(False)

    c = db.cursor()

    print 'searching for target: {0}'.format(target)

    matches, t = search(c, target, float(threshold))
    for match in matches:
        print match[0], match[1], match[2]
    print ('Found {0} matches in {1} seconds').format(len(matches), t)

    db.close()

if __name__=="__main__":
    tanimoto_search(*sys.argv[1:5])
