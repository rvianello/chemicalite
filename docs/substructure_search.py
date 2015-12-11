#!/bin/env python
from __future__ import print_function
import sys
import time

import apsw

from rdkit import Chem

def search(c, substructure):
    t1 = time.time()
    rs = c.execute("select chembl.chembl_id, chembl.smiles from "
                   "chembl, str_idx_chembl_molecule as idx where "
                   "chembl.id = idx.id and "
                   "mol_is_substruct(chembl.molecule, ?) and "
                   "idx.id match rdtree_subset(mol_bfp_signature(?)) "
                   "limit 25",
                   (substructure, substructure)).fetchall()
    t2 = time.time()
    return rs, t2-t1

def substructure_search(chemicalite_path, chembldb_sql, substructure):
    connection = apsw.Connection(chembldb_sql)
    connection.enableloadextension(True)
    connection.loadextension(chemicalite_path)
    connection.enableloadextension(False)

    c = connection.cursor()

    print('searching for substructure:', substructure)

    matches, t = search(c, substructure)
    for match in matches:
        print(match[0], match[1])
    print('Found {0} matches in {1} seconds'.format(len(matches), t))

    
if __name__=="__main__":
    substructure_search(*sys.argv[1:4])
