#!/bin/env python3
import argparse
import time

import sqlite3

def substructure_search(connection, substructure, limit):
    print('searching for substructure:', substructure)

    t1 = time.time()
    rs = connection.execute(
        "select chembl.chembl_id, mol_to_smiles(chembl.molecule) from "
        "chembl, str_idx_chembl_molecule as idx where "
        "chembl.id = idx.id and "
        "mol_is_substruct(chembl.molecule, mol_from_smiles(?1)) and "
        "idx.id match rdtree_subset(mol_pattern_bfp(mol_from_smiles(?1), 2048)) "
        "limit ?2",
        (substructure, limit)).fetchall()
    t2 = time.time()

    for chembl_id, smiles in rs:
        print(chembl_id, smiles)
    print('Found {0} matches in {1} seconds'.format(len(rs), t2-t1))

    
if __name__=="__main__":

    parser= argparse.ArgumentParser(
        description='Return the first N records matching the input substructure')
    parser.add_argument('chembldb',
        help='The path to the SQLite database w/ the ChEMBL compounds')
    parser.add_argument('smiles', help='The input substructure in SMILES format')
    parser.add_argument('--chemicalite', default='chemicalite',
        help='The name or path to the ChemicaLite extension module')
    parser.add_argument('--limit', default=25, type=int,
        help='The maximum number of output results')
    
    args = parser.parse_args()

    connection = sqlite3.connect(args.chembldb)
    connection.enable_load_extension(True)
    connection.load_extension(args.chemicalite)
    connection.enable_load_extension(False)

    substructure_search(connection, args.smiles, args.limit)
