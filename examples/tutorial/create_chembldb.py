#!/bin/env python3
import argparse
import csv
import itertools
import sqlite3


def chembl(path):
    with open(path, 'rt') as inputfile:
        reader = csv.reader(inputfile, delimiter='\t')
        next(reader) # skip header line
        for chembl_id, smiles, *_ in reader:
            yield chembl_id, smiles

def load_chembl(connection, chembl_chemreps, limit):

    chembl_data = chembl(chembl_chemreps)
    if limit is not None:
        chembl_data = itertools.islice(chembl_data, limit)
    
    connection.execute(
        "CREATE TABLE chembl(id INTEGER PRIMARY KEY, chembl_id TEXT, molecule MOL)")

    with connection:
        connection.executemany(
            "INSERT INTO chembl(chembl_id, molecule) "
            "VALUES(?1, mol_from_smiles(?2))", chembl_data)

    connection.execute("CREATE VIRTUAL TABLE str_idx_chembl_molecule " +
                "USING rdtree(id, fp bits(2048))")

    with connection:
        connection.execute( 
            "INSERT INTO str_idx_chembl_molecule(id, fp) " + 
            "SELECT id, mol_pattern_bfp(molecule, 2048) FROM chembl " + 
            "WHERE molecule IS NOT NULL")

    # create a virtual table to be filled with morgan bfp data
    connection.execute("CREATE VIRTUAL TABLE morgan_idx_chembl_molecule " +
                "USING rdtree(id, fp bits(1024))");

    with connection:
        connection.execute( 
            "INSERT INTO morgan_idx_chembl_molecule(id, fp) " + 
            "SELECT id, mol_morgan_bfp(molecule, 2, 1024) FROM chembl " + 
            "WHERE molecule IS NOT NULL")

    connection.close()

if __name__=="__main__":

    parser= argparse.ArgumentParser(
        description='Load an SQLite database with the structures from ChEMBL')
    parser.add_argument('chembldb',
        help='The path to the SQLite database w/ the ChEMBL compounds')
    parser.add_argument('chembl_chemreps',
        help='The path to the ChEMBL input file')
    parser.add_argument('--chemicalite', default='chemicalite',
        help='The name or path to the ChemicaLite extension module')
    parser.add_argument('--limit', type=int,
        help='Limit the number of compounds in the output database')

    args = parser.parse_args()

    connection = sqlite3.connect(args.chembldb)
    connection.enable_load_extension(True)
    connection.load_extension(args.chemicalite)
    connection.enable_load_extension(False)

    load_chembl(connection, args.chembl_chemreps, args.limit)
