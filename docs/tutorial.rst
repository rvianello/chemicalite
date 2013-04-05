ChemicaLite Tutorial
====================

Building a database
-------------------

This tutorial will guide you through the construction of a chemical database using SQLite and the ChemicaLite extension. Python will be used in illustrating the various operations, but almost any other programming language could be used instead (as long as SQLite drivers are available).

Download a copy of the `ChEMBLdb database <ftp://ftp.ebi.ac.uk/pub/databases/chembl/ChEMBLdb/releases/chembl_15/chembl_15_chemreps.txt.gz>`_ and decompress it::

    $ gunzip chembl_15_chemreps.txt.gz

Creating a database and initializing its schema requires just a few statements::

    # we need custom drivers that support loading an SQLite extension
    # (this feature is usually disabled in the normal python distribution)
    from pysqlite2 import dbapi2 as sqlite3
    
    # the extension is usually loaded right after the connection to the
    # database
    db = sqlite3.connect('chembl.sql')
    db.enable_load_extension(True)
    db.load_extension(chemicalite_path)
    db.enable_load_extension(False)
    
    # the SQLite memory page size affects the configuration of the
    # substructure search index tree. this operation must be performed
    # at database creation, before the first CREATE TABLE.
    db.execute("PRAGMA page_size=2048")
    
    # our database will consist of a single table, containing a subset of the
    # columns from the ChEMBLdb database. The molecular structure is inserted
    # as a binary blob of the pickled molecule.
    db.execute("CREATE TABLE chembl(id INTEGER PRIMARY KEY, "
               "chembl_id TEXT, smiles TEXT, molecule MOL)")
	       	       
    # finally, this statement will create and configure an index
    # associated to the 'molecule' column of the 'chembl' table.       
    db.execute("SELECT create_molecule_rdtree('chembl', 'molecule')")

Support for custom indexes in SQLite is a bit different than other database engines. The data structure of a custom index is in fact wrapped behind the implementation of a so-called "virtual table", an object that exposes an interface that is almost identical to that of a regular SQL table, but whose implementation can be customized.

The above call to the `create_molecule_rdtree` function creates a virtual table with SQL name `str_idx_chembl_molecule` and a few triggers that connect the manipulation of the `molecule` column of the `chembl` table with the management of the tree data structure wrapped behind `str_idx_chembl_molecule`.

For example, each time a new record is inserted into the `chembl` table, a bitstring signature of the involved molecule is computed and inserted into `str_idx_chembl_molecule`. 

Join operations involving the `chembl` and `str_idx_chembl_molecule` tables can this way use the tree data structure to strongly reduce the number of `chembl` records that are checked during a substructure search. 

The ChEMBLdb data can be parsed with a python generator function similar to the following::

    def chembl(path):
        """Extract the chembl_id and SMILES fields"""
        with open(path, 'rb') as inputfile:
            reader = csv.reader(inputfile, delimiter='\t',
                                skipinitialspace=True)
            reader.next() # skip header line
            
            for chembl_id, chebi_id, smiles, inchi, inchi_key in reader:
                # check the SMILES and skip problematic compounds
                # [...]
                yield chembl_id, smiles

And the database is loaded with loop like this::

    c = db.cursor()
    for chembl_id, smiles in chembl(chembl_path):
        c.execute("INSERT INTO chembl(chembl_id, smiles, molecule) "
                  "VALUES(?, ?, mol(?))", (chembl_id, smiles, smiles))
    db.commit()
    db.close()

Please note that loading the whole ChEMBLdb is going to take a substantial amount of time (about one to two hours depending on the available computational power) and the resulting file will require about 1.5GB of disk space.

A python script implementing the full schema creation and database loading procedure as a single command line tool is available in the `docs` directory of the source code distribution::

    # This will create the molecular database as a file named 'chembldb.sql'
    $ ./create_chembldb.py /path/to/libchemicalite.so chembl_15_chemreps.txt

Substructure Searches
---------------------

A search for substructures could be performed with a query like the following::

    SELECT COUNT(*) FROM chembl WHERE mol_is_substruct(molecule, 'c1ccnnc1');

but this would check every single record of the `chembl` table, resulting very inefficient. Performances can strongly improve if the index table is joined::

    SELECT COUNT(*) FROM chembl, str_idx_chembl_molecule AS idx WHERE
        chembl.id = idx.id AND 
        mol_is_substruct(chembl.molecule, 'c1ccnnc1') AND
        idx.id MATCH rdtree_subset(mol_bfp_signature('c1ccnnc1'));

A python script executing this second query is available in the `docs` directory of the source code distribution::

    # returns the number of structures containing the query fragment.
    $ ./match_count.py /path/libchemicalite.so /path/to/chembldb.sql c1ccnnc1

And here are some example queries::

    $ ./match_count.py /path/libchemicalite.so chembldb.sql c1cccc2c1nncc2
    searching for substructure: c1cccc2c1nncc2
    Found 285 matches in 0.580219984055 seconds

    $ ./match_count.py /path/libchemicalite.so chembldb.sql c1ccnc2c1nccn2
    searching for substructure: c1ccnc2c1nccn2
    Found 707 matches in 0.415385007858 seconds

    $ ./match_count.py /path/libchemicalite.so chembldb.sql Nc1ncnc\(N\)n1
    searching for substructure: Nc1ncnc(N)n1
    Found 4564 matches in 1.44142603874 seconds
    
    $ ./match_count.py /path/libchemicalite.so chembldb.sql c1scnn1
    searching for substructure: c1scnn1
    Found 11235 matches in 2.81160211563 seconds
    
    $ ./match_count.py /path/libchemicalite.so chembldb.sql c1cccc2c1ncs2
    searching for substructure: c1cccc2c1ncs2
    Found 13521 matches in 5.35551190376 seconds
    
    $ ./match_count.py /path/libchemicalite.so chembldb.sql c1cccc2c1CNCCN2
    searching for substructure: c1cccc2c1CNCCN2
    Found 1210 matches in 15.256114006 seconds

A second script is provided with the documentation and it's designed to only return the first results (sometimes useful for queries that return a large number of matches)::

    $ ./substructure_search.py /path/libchemicalite.so chembldb.sql c1cccc2c1CNCCN2
    searching for substructure: c1cccc2c1CNCCN2
    CHEMBL323692 C1CNc2ccccc2CN1
    CHEMBL1458895 COC(=O)CN1CCN(C(=O)c2ccc(F)cc2)c3ccccc3C1
    CHEMBL1623831 C(C1CNc2ccccc2CN1)c3ccccc3
    [...]
    CHEMBL270270 NCCCCC1NC(=O)c2ccc(Cl)cc2N(Cc3ccccc3)C1=O
    CHEMBL233255 Oc1ccc(C[C@@H]2NC(=O)c3ccccc3NC2=O)cc1
    Found 25 matches in 0.536008834839 seconds
