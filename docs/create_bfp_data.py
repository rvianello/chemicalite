#!/bin/env python
from __future__ import print_function

import sys
import csv

import apsw

def createbfp(chemicalite_path, chembldb_path):
    '''Create indexed virtual tables containing the bfp data'''

    connection = apsw.Connection(chembldb_path)
    connection.enableloadextension(True)
    connection.loadextension(chemicalite_path)
    connection.enableloadextension(False)

    cursor = connection.cursor()
    
    # sorry for the hard-coded bfp sizes in bytes (128, 64). 
    # I will fix this
    cursor.execute("CREATE VIRTUAL TABLE torsion USING rdtree(id, bfp bytes(128))");
    cursor.execute("CREATE VIRTUAL TABLE morgan USING rdtree(id, bfp bytes(64))");
    cursor.execute("CREATE VIRTUAL TABLE feat_morgan USING rdtree(id, bfp bytes(64))");

    cursor.execute("INSERT INTO torsion(id, bfp) SELECT id, mol_topological_torsion_bfp(molecule) FROM chembl")
    cursor.execute("INSERT INTO morgan(id, bfp) SELECT id, mol_morgan_bfp(molecule, 2) FROM chembl")
    cursor.execute("INSERT INTO feat_morgan(id, bfp) SELECT id, mol_feat_morgan_bfp(molecule, 2) FROM chembl")

if __name__=="__main__":
    if len(sys.argv) == 3:
        createbfp(*sys.argv[1:3])
    else:
        print('Usage: {0} <path to chemicalite.so> <path to chembldb sqlite file>'.format(sys.argv[0]))
