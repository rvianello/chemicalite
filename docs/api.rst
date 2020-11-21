API Reference
=============

Data types
----------

* `mol`: an rdkit molecule. Can be created from a SMILES using the `mol` function, for example: `mol('c1ccccc1')` creates a molecule from the SMILES `'c1ccccc1'`
* `qmol`: an rdkit molecule containing query features (i.e. constructed from SMARTS). Can be created from a SMARTS using the `qmol` function, for example: `qmol('c1cccc[c,n]1')` creates a query molecule from the SMARTS `'c1cccc[c,n]1'`
* `bfp`: a bit vector fingerprint

In most places where a `mol` or `qmol` is expected, a text string can be used instead and is implicitly interpreted as a SMILES. Use an explicit cast in case a SMARTS is used.

.. note::
  All of the above data types are serialized and stored in the form of binary blobs. No attempt is made at implementing any kind of type system that could allow the code to recognize if the wrong data is passed as input to a procedure. Some care is therefore recommended.

Operators
---------

Substructure searches
.....................

Substructure searches are performed constraining the selection on a column of `mol` data with a `WHERE` clause based on the return value of function `mol_is_substruct`. This can be optionally (and preferably) joined with a `MATCH` constraint on an `rdtree` index, using the match object returned by `rdtree_subset`::

    SELECT * FROM mytable, str_idx_mytable_molcolumn AS idx WHERE
        mytable.id = idx.id AND 
        mol_is_substruct(mytable.molcolumn, 'c1ccnnc1') AND
        idx.id MATCH rdtree_subset(mol_bfp_signature('c1ccnnc1'));

Similarity searches
...................

Similarity search on `rdtree` virtual tables of binary fingerprint data are supported by means of the match object returned by the `rdtree_tanimoto` factory function::

    SELECT c.smiles, bfp_tanimoto(mol_morgan_bfp(c.molecule, 2), mol_morgan_bfp(?, 2)) as t
        FROM mytable as c JOIN (SELECT id FROM morgan WHERE id match rdtree_tanimoto(mol_morgan_bfp(?, 2), ?)) as idx
        USING(id) ORDER BY t DESC;

Functions
---------

Molecule
........

* `mol(string)`
* `qmol(string)`
* `mol_smiles(mol)`
* `mol_is_substruct(mol, mol)`
* `mol_is_superstruct(mol, mol)`
* `mol_cmp(mol, mol)`

Descriptors
...........

* `mol_hba(mol)`
* `mol_hbd(mol)`
* `mol_mw(mol)`
* `mol_logp(mol)`
* `mol_tpsa(mol)`
* `mol_num_atms(mol)`
* `mol_num_hvyatms(mol)`
* `mol_num_rotatable_bnds(mol)`
* `mol_num_hetatms(mol)`
* `mol_num_rings(mol)`
* `mol_chi0v(mol)` - `mol_chi4v(mol)`
* `mol_chi0n(mol)` - `mol_chi4n(mol)`
* `mol_kappa1(mol)` - `mol_kappa3(mol)`

Fingerprints
............

* `mol_layered_bfp(mol)`
* `mol_rdkit_bfp(mol)`
* `mol_atom_pairs_bfp(mol)`
* `mol_topological_torsion_bfp(mol)`
* `mol_maccs_bfp(mol)`
* `mol_morgan_bfp(mol, int)`
* `mol_feat_morgan_bfp(mol, int)`
* `mol_bfp_signature(mol)`
* `bfp_tanimoto(bfp, bfp)`
* `bfp_dice(bfp, bfp)`
* `bfp_length(bfp)`
* `bfp_weight(bfp)`
