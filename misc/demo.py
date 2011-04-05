from pysqlite2 import dbapi2 as sqlite3

db = sqlite3.connect('chembl.sql')
db.enable_load_extension(True)

# <demo> stop

db.load_extension('libsigntree.so')
db.load_extension('libchemicalite.so')

# <demo> stop

db.execute("CREATE TABLE chembl("
           "id INTEGER PRIMARY KEY, chembl_id TEXT, smiles TEXT, mol MOLECULE"
           ")")

# <demo> stop

db.execute("SELECT mol_structural_index('chembl', 'mol')")

# <demo> stop

def read_chembldb(filepath, limit=0):
    import csv
    from rdkit import Chem

    count = 0
    inputfile = open(filepath, 'rb')
    reader = csv.reader(inputfile, delimiter='\t', skipinitialspace=True)
    reader.next() # skip header line

    for chembl_id, chebi_id, mw, smiles, inchi, inchi_key in reader:

        # skip problematic compounds
        if len(smiles) > 300: continue
        smiles = smiles.replace('=N#N','=[N+]=[N-]')
        smiles = smiles.replace('N#N=','[N-]=[N+]=')
        if not Chem.MolFromSmiles(smiles): continue

        yield chembl_id, smiles, smiles # smiles repeated to match schema

        count +=1
        if limit > 0 and count == limit: break

# <demo> stop

db.executemany("INSERT INTO chembl(chembl_id, smiles, mol) "
               "VALUES(?, ?, mol(?));",
               read_chembldb('chembl_08_chemreps.txt', limit=50000))
db.commit()

# <demo> stop

def search_substruct(substr):
    return db.execute("SELECT chembl.chembl_id, chembl.smiles FROM "
                      "chembl, str_idx_chembl_mol AS idx WHERE "
                      "mol_is_substruct(chembl.mol, ?) AND "
                      "chembl.id = idx.id AND idx.s > mol_signature(?)",
                      (substr, substr))

# <demo> stop

