#!/bin/env python
import sys
import time

from pysqlite2 import dbapi2 as sqlite3

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
    db = sqlite3.connect(chembldb_sql)
    db.enable_load_extension(True)
    db.load_extension(chemicalite_path)
    db.enable_load_extension(False)

    c = db.cursor()

    print 'searching for substructure: {0}'.format(substructure)

    matches, t = search(c, substructure)
    for match in matches:
        print match[0], match[1]
    print ('Found {0} matches in {1} seconds').format(len(matches), t)

    db.close()

if __name__=="__main__":
    substructure_search(*sys.argv[1:4])
