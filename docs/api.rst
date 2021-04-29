API Reference
=============

Data types
----------

* `mol`: an RDKit molecule.
* `bfp`: a binary fingerprint.

The `mol` type is used to represent both a "regular" fully-specified molecule, and also a molecular structure that includes query features (e.g. built from SMARTS input).

No implicit conversion from text input formats to `mol` is supported. Passing a SMILES or SMARTS string where a `mol` argument is expected, should result in an error. The input textual representation is always required to be wrapped by a suitable conversion function (e.g. `mol_from_smiles`).

Functions
---------

Molecule
........

* `mol_from_smiles(text) -> mol`
* `mol_from_smarts(text) -> mol`
* `mol_from_molblock(text) -> mol`
* `mol_from_binary_mol(blob) -> mol`

..

* `mol_to_smiles(mol) -> text`
* `mol_to_smarts(mol) -> text`
* `mol_to_molblock(mol) -> text`
* `mol_to_binary_mol(mol) -> blob`

..

* `mol_is_substruct(mol, mol) -> int`
* `mol_is_superstruct(mol, mol) -> int`
* `mol_cmp(mol, mol) -> int`

..

* `mol_hba(mol) -> int`
* `mol_hbd(mol) -> int`
* `mol_num_atms(mol) -> int`
* `mol_num_hvyatms(mol) -> int`
* `mol_num_rotatable_bnds(mol) -> int`
* `mol_num_hetatms(mol) -> int`
* `mol_num_rings(mol) -> int`
* `mol_num_aromatic_rings(mol) -> int`
* `mol_num_aliphatic_rings(mol) -> int`
* `mol_num_saturated_rings(mol) -> int`

..

* `mol_mw(mol) -> real`
* `mol_tpsa(mol) -> real`
* `mol_fraction_csp3(mol) -> real`
* `mol_chi0v(mol)` - `mol_chi4v(mol) -> real`
* `mol_chi0n(mol)` - `mol_chi4n(mol) -> real`
* `mol_kappa1(mol)` - `mol_kappa3(mol) -> real`
* `mol_logp(mol) -> real`

..

* `mol_formula(mol) -> text`

..

* `mol_hash_anonymousgraph(mol) -> text`
* `mol_hash_elementgraph(mol) -> text`
* `mol_hash_canonicalsmiles(mol) -> text`
* `mol_hash_murckoscaffold(mol) -> text`
* `mol_hash_extendedmurcko(mol) -> text`
* `mol_hash_molformula(mol) -> text`
* `mol_hash_atombondcounts(mol) -> text`
* `mol_hash_degreevector(mol) -> text`
* `mol_hash_mesomer(mol) -> text`
* `mol_hash_hetatomtautomer(mol) -> text`
* `mol_hash_hetatomprotomer(mol) -> text`
* `mol_hash_redoxpair(mol) -> text`
* `mol_hash_regioisomer(mol) -> text`
* `mol_hash_netcharge(mol) -> text`
* `mol_hash_smallworldindexbr(mol) -> text`
* `mol_hash_smallworldindexbrl(mol) -> text`
* `mol_hash_arthorsubstructureorder(mol) -> text`

Binary Fingerprints
...................

* `mol_layered_bfp(mol) -> bfp`
* `mol_layered_bfp(mol, int) -> bfp`
* `mol_rdkit_bfp(mol) -> bfp`
* `mol_rdkit_bfp(mol, int) -> bfp`
* `mol_atom_pairs_bfp(mol) -> bfp`
* `mol_atom_pairs_bfp(mol, int) -> bfp`
* `mol_topological_torsion_bfp(mol) -> bfp`
* `mol_topological_torsion_bfp(mol, int) -> bfp`
* `mol_pattern_bfp(mol) -> bfp`
* `mol_pattern_bfp(mol, int) -> bfp`
* `mol_maccs_bfp(mol) -> bfp`
* `mol_morgan_bfp(mol, int) -> bfp`
* `mol_morgan_bfp(mol, int, int) -> bfp`
* `mol_feat_morgan_bfp(mol, int) -> bfp`
* `mol_feat_morgan_bfp(mol, int, int) -> bfp`

..

* `bfp_tanimoto(bfp, bfp) -> real`
* `bfp_dice(bfp, bfp) -> real`

..

* `bfp_length(bfp) -> int`
* `bfp_weight(bfp) -> int`

Utility
.......

* `chemicalite_version() -> text`
* `rdkit_version() -> text`
* `rdkit_build() -> text`
* `boost_version() -> text`
  
Substructure queries
--------------------

Substructure searches are performed constraining the selection on a column of `mol` data with a `WHERE` clause based on the return value of function `mol_is_substruct`. This can be optionally (and preferably) joined with a `MATCH` constraint on an `rdtree` index, using the match object returned by `rdtree_subset`::

    SELECT * FROM mytable, str_idx_mytable_molcolumn AS idx WHERE
        mytable.id = idx.id AND 
        mol_is_substruct(mytable.molcolumn, 'c1ccnnc1') AND
        idx.id MATCH rdtree_subset(mol_bfp_signature('c1ccnnc1'));

Similarity queries
------------------

Similarity search on `rdtree` virtual tables of binary fingerprint data are supported by means of the match object returned by the `rdtree_tanimoto` factory function::

    SELECT c.smiles, bfp_tanimoto(mol_morgan_bfp(c.molecule, 2), mol_morgan_bfp(?, 2)) as t
        FROM mytable as c JOIN (SELECT id FROM morgan WHERE id match rdtree_tanimoto(mol_morgan_bfp(?, 2), ?)) as idx
        USING(id) ORDER BY t DESC;

