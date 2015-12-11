#!/bin/env python
from __future__ import print_function
import sys
import time

import apsw

from rdkit import Chem

def search(c, substructure):
    t1 = time.time()
    count = c.execute("SELECT count(*) FROM "
                      "chembl, str_idx_chembl_molecule as idx WHERE "
                      "chembl.id = idx.id AND "
                      "mol_is_substruct(chembl.molecule, ?) AND "
                      "idx.id match rdtree_subset(mol_bfp_signature(?))",
                      (substructure, substructure)).fetchall()[0][0]
    t2 = time.time()
    return count, t2-t1


def match_count(chemicalite_path, chembldb_sql, substructure):
    connection = apsw.Connection(chembldb_sql)
    connection.enableloadextension(True)
    connection.loadextension(chemicalite_path)
    connection.enableloadextension(False)

    c = connection.cursor()

    print('searching for substructure:', substructure)

    count, t = search(c, substructure)
    print('Found {0} matches in {1} seconds'.format(count, t))

    
if __name__=="__main__":
    match_count(*sys.argv[1:4])
