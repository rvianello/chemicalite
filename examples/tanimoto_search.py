#!/bin/env python3
import argparse
import time

import sqlite3

def tanimoto_search(connection, target, threshold):
    print('searching for target:', target)

    t1 = time.time()
    rs = connection.execute(
        "SELECT c.chembl_id, mol_to_smiles(c.molecule), "
        "bfp_tanimoto(mol_morgan_bfp(c.molecule, 2, 1024), "
        "             mol_morgan_bfp(mol_from_smiles(?1), 2, 1024)) as t "
        "FROM "
        "chembl as c JOIN morgan_idx_chembl_molecule as idx USING(id) "
        "WHERE "
        "idx.id MATCH rdtree_tanimoto(mol_morgan_bfp(mol_from_smiles(?1), 2, 1024), ?2) "
        "ORDER BY t DESC",
        (target, threshold)).fetchall()
    t2 = time.time()

    for chembl_id, smiles, sim in rs:
        print(chembl_id, smiles, sim)

    print('Found {0} matches in {1} seconds'.format(len(rs), t2-t1))


if __name__=="__main__":
    parser= argparse.ArgumentParser(
        description='Find the compounds similar to the input structure')
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

    tanimoto_search(connection, args.smiles, args.threshold)

    connection.close()
