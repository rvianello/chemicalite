#!/bin/env python3
import argparse
import time

import sqlite3


def match_count(connection, substructure):

    print('searching for substructure:', substructure)
    t1 = time.time()

    count = connection.execute(
        "SELECT count(*) FROM "
        "chembl, str_idx_chembl_molecule as idx WHERE "
        "chembl.id = idx.id AND "
        "mol_is_substruct(chembl.molecule, mol_from_smiles(?1)) AND "
        "idx.id match rdtree_subset(mol_pattern_bfp(mol_from_smiles(?1), 2048))",
        (substructure,)
        ).fetchall()[0][0]

    t2 = time.time()
    print('Found {0} matching structures in {1} seconds'.format(count, t2-t1))

    
if __name__=="__main__":

    parser= argparse.ArgumentParser(
        description='Find the number of records matching the input substructure')
    parser.add_argument('chembldb',
        help='The path to the SQLite database w/ the ChEMBL compounds')
    parser.add_argument('smiles', help='The input substructure in SMILES format')
    parser.add_argument('--chemicalite', default='chemicalite',
        help='The name or path to the ChemicaLite extension module')
    
    args = parser.parse_args()

    connection = sqlite3.connect(args.chembldb)
    connection.enable_load_extension(True)
    connection.load_extension(args.chemicalite)
    connection.enable_load_extension(False)

    match_count(connection, args.smiles)

    connection.close()
