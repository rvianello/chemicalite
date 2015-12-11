#!/bin/env python
from __future__ import print_function

import sys
import time

import apsw

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
    connection = apsw.Connection(chembldb_sql)
    connection.enableloadextension(True)
    connection.loadextension(chemicalite_path)
    connection.enableloadextension(False)

    cursor = connection.cursor()

    print('searching for target:', target)

    matches, t = search(cursor, target, float(threshold))
    for match in matches:
        print(match[0], match[1], match[2])
    print('Found {0} matches in {1} seconds'.format(len(matches), t))


if __name__=="__main__":
    tanimoto_search(*sys.argv[1:5])
