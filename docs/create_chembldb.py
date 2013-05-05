#!/bin/env python
import sys
import csv

from pysqlite2 import dbapi2 as sqlite3

from rdkit import Chem

def chembl(path, limit=None):
    '''Parse the ChEMBLdb CSV format and return the chembl_id, smiles fields'''

    with open(path, 'rb') as inputfile:
        reader = csv.reader(inputfile, delimiter='\t', skipinitialspace=True)
        reader.next() # skip header line
        
        counter = 0
        for chembl_id, chebi_id, smiles, inchi, inchi_key in reader:
            
            # skip problematic compounds
            if len(smiles) > 300: continue
            smiles = smiles.replace('=N#N','=[N+]=[N-]')
            smiles = smiles.replace('N#N=','[N-]=[N+]=')
            if not Chem.MolFromSmiles(smiles): continue
            
            yield chembl_id, smiles
            counter += 1
            if counter == limit:
                break

def createdb(chemicalite_path, chembl_path):
    '''Initialize a database schema and load the ChEMBLdb data'''

    db = sqlite3.connect('chembldb.sql')
    db.enable_load_extension(True)
    db.load_extension(chemicalite_path)
    db.enable_load_extension(False)

    db.execute("PRAGMA page_size=4096")

    db.execute("CREATE TABLE chembl(id INTEGER PRIMARY KEY, "
               "chembl_id TEXT, smiles TEXT, molecule MOL)")

    db.execute("SELECT create_molecule_rdtree('chembl', 'molecule')")

    c = db.cursor()

    for chembl_id, smiles in chembl(chembl_path):
        c.execute("INSERT INTO chembl(chembl_id, smiles, molecule) "
                  "VALUES(?, ?, mol(?))", (chembl_id, smiles, smiles))

    db.commit()
    db.close()

if __name__=="__main__":
    if len(sys.argv) == 3:
        createdb(*sys.argv[1:3])
    else:
        print ('Usage: {0} <path to libchemicalite.so> '
               '<path to chembl_15_chemreps.txt>').format(sys.argv[0])
