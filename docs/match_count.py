#!/bin/env python
import sys
import time

from pysqlite2 import dbapi2 as sqlite3

from rdkit import Chem

def search(c, substructure):
    t1 = time.time()
    count = c.execute("select count(*) from "
                      "chembl, str_idx_chembl_molecule as idx where "
                      "chembl.id = idx.id and "
                      "mol_is_substruct(chembl.molecule, ?) and "
                      "idx.id match rdtree_subset(mol_bfp_signature(?))",
                      (substructure, substructure)).fetchone()[0]
    t2 = time.time()
    return count, t2-t1

def match_count(chemicalite_path, chembldb_sql, substructure):
    db = sqlite3.connect(chembldb_sql)
    db.enable_load_extension(True)
    db.load_extension(chemicalite_path)
    db.enable_load_extension(False)

    c = db.cursor()

    print 'searching for substructure: {0}'.format(substructure)

    count, t = search(c, substructure)
    print ('Found {0} matches in {1} seconds').format(count, t)

    db.close()

if __name__=="__main__":
    match_count(*sys.argv[1:4])
