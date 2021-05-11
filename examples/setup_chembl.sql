-- This script provides an example of how to extend the SQLite version of the ChEMBL database
-- to include a table of RDKit molecule instances, and two support RD-tree index tables that
-- can be used to speed-up substructure and similarity queries.
-- 
-- The script can be loaded and executed directly inside the SQLite shell:
--
-- $ sqlite3 ./chembl_28.db
-- sqlite> .read path/to/this_file.sql
--
-- This script was tested with ChEMBL 28, ChemicaLite 2021.05.2, RDKit 2021.03.1
-- (it may not work with any later releases)

-- Load the ChemicaLite extension
-- you may need to adjust this line (e.g. replace the extension name with the path
-- to the module file) if chemicalite is not installed under the dynamic loader
-- search path.
.load chemicalite

BEGIN;

-- Create a table of RDKit molecules
CREATE TABLE rdkit_molecules(id INTEGER PRIMARY KEY, molregno BIGINT NOT NULL, mol MOL);
INSERT INTO rdkit_molecules(molregno, mol) SELECT molregno, mol_from_smiles(canonical_smiles) FROM compound_structures;

-- Create an index for substructure queries
CREATE VIRTUAL TABLE rdkit_structure_index USING rdtree(id, s bits(2048));
INSERT INTO rdkit_structure_index (id, s) SELECT id, mol_pattern_bfp(mol, 2048) FROM rdkit_molecules WHERE mol IS NOT NULL;

-- Create an index for similarity queries
CREATE VIRTUAL TABLE rdkit_similarity_index USING rdtree(id, s bits(2048));
INSERT INTO rdkit_similarity_index (id, s) SELECT id, mol_morgan_bfp(mol, 2, 2048) FROM rdkit_molecules WHERE mol IS NOT NULL;

COMMIT;

-- Give SQLite the opportunity to optimize the database (e.g. run ANALYZE) if needed
PRAGMA optimize;

-- Exit the SQLite shell
.quit

