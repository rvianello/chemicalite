#!/bin/env python
import sys
import csv

from pysqlite2 import dbapi2 as sqlite3

def createbfp(chemicalite_path, chembldb_path):
    '''Create indexed virtual tables containing the bfp data'''

    db = sqlite3.connect(chembldb_path)
    db.enable_load_extension(True)
    db.load_extension(chemicalite_path)
    db.enable_load_extension(False)

    # sorry for the hard-coded bfp sizes in bytes (128, 64). 
    # I will fix this
    db.execute("CREATE VIRTUAL TABLE torsion USING\n"
               "rdtree(id, bfp bytes(128))");
    db.execute("CREATE VIRTUAL TABLE morgan USING\n"
               "rdtree(id, bfp bytes(64))");
    db.execute("CREATE VIRTUAL TABLE feat_morgan USING\n"
               "rdtree(id, bfp bytes(64))");

    db.execute("INSERT INTO torsion(id, bfp)\n"
               "SELECT id, mol_topological_torsion_bfp(molecule) FROM chembl")
    db.execute("INSERT INTO morgan(id, bfp)\n"
               "SELECT id, mol_morgan_bfp(molecule, 2) FROM chembl")
    db.execute("INSERT INTO feat_morgan(id, bfp)\n"
               "SELECT id, mol_feat_morgan_bfp(molecule, 2) FROM chembl")

    db.commit()
    db.close()

if __name__=="__main__":
    if len(sys.argv) == 3:
        createbfp(*sys.argv[1:3])
    else:
        print ('Usage: {0} <path to libchemicalite.so> '
               '<path to chembldb sqlite file>').format(sys.argv[0])
