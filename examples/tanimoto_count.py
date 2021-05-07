#!/bin/env python3
import argparse
import time

import sqlite3

def search(c, target, threshold):
    return count, t2-t1

def tanimoto_count(connection, target, threshold):
    print('Target structure:', target)
    print('Minimum Tanimoto similarity:', threshold)

    t1 = time.time()
    count = connection.execute(
        "SELECT count(*) FROM "
        "morgan_idx_chembl_molecule as idx WHERE "
        "idx.id match rdtree_tanimoto(mol_morgan_bfp(mol_from_smiles(?), 2, 1024), ?)",
        (target, threshold)).fetchall()[0][0]
    t2 = time.time()

    print('Found {0} matching objects in {1} seconds'.format(count, t2-t1))


if __name__=="__main__":

    parser= argparse.ArgumentParser(
        description='Find the number of records similar to the input structure')
    parser.add_argument('chembldb',
        help='The path to the SQLite database w/ the ChEMBL compounds')
    parser.add_argument('smiles', help='The input structure in SMILES format')
    parser.add_argument('threshold', type=float,
        help='The minimum similarity of the matching objects')
    parser.add_argument('--chemicalite', default='chemicalite',
        help='The name or path to the ChemicaLite extension module')
    
    args = parser.parse_args()

    connection = sqlite3.connect(args.chembldb)
    connection.enable_load_extension(True)
    connection.load_extension(args.chemicalite)
    connection.enable_load_extension(False)

    tanimoto_count(connection, args.smiles, args.threshold)

    connection.close()
