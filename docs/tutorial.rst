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
    # columns from the ChEMBLdb database.
    db.execute("CREATE TABLE chembl(id INTEGER PRIMARY KEY, "
               "chembl_id TEXT, smiles TEXT, molecule MOL)")
	       	       
    # finally, this statement will create and configure an index
    # associated to the 'molecule' column of the 'chembl' table.       
    db.execute("SELECT create_molecule_rdtree('chembl', 'molecule')")


Substructure Searches
---------------------

