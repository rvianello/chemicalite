ChemicaLite Tutorial
====================

Building a database
-------------------

This tutorial is based on a similar one which is part of the `RDKit PostgreSQL Cartridge documentation <https://rdkit.readthedocs.org/en/latest/Cartridge.html#creating-databases>`_ and it will guide you through the construction of a chemical SQLite database, and the execution of some simple queries. Python will be used in illustrating the various operations, but almost any other programming language could be used instead (as long as SQLite drivers are available).

Download a copy of the `ChEMBLdb database <https://ftp.ebi.ac.uk/pub/databases/chembl/ChEMBLdb/releases/chembl_31/chembl_31_chemreps.txt.gz>`_ and decompress it::

    $ gunzip chembl_31_chemreps.txt.gz

Creating a database and initializing its schema requires just a few statements::

    import sqlite3
    
    connection = sqlite3.connect('chembldb.sql')

    # the extension is usually loaded right after the connection to the
    # database

    # because this operation loads external code from the extension module into
    # the program, it's considered potentially insecure, and it's by default disabled.
    # it therefore needs to be explicitly enabled (and then disabled again).
    connection.enable_load_extension(True)

    # if the chemicalite extension is installed under the dynamic library search path
    # for the system or running process, you can simply refer to it by name.
    # otherwise you may need to pass the filesystem path of the loadable module file:
    # connection.load_extension('/path/to/chemicalite.so')
    connection.load_extension('chemicalite')
 
    connection.enable_load_extension(False)

    # the database will mainly consist of one table, containing the
    # compound structures from the ChEMBLdb data.
    connection.execute(
        "CREATE TABLE chembl(id INTEGER PRIMARY KEY, chembl_id TEXT, molecule MOL)")

The ChEMBLdb compounds data is available as a simple tsv file, that can be parsed with a python generator function similar to the following::

    import csv

    def chembl(path):
        with open(path, 'rt') as inputfile:
            reader = csv.reader(inputfile, delimiter='\t')
            next(reader) # skip header line
            for chembl_id, smiles, *_ in reader:
                yield chembl_id, smiles

And the database table can be loaded with a statement like this::

    with connection:
        connection.executemany(
            "INSERT INTO chembl(chembl_id, molecule) "
            "VALUES(?1, mol_from_smiles(?2))", chembl('chembl_31_chemreps.txt'))

A complete script implementing the full creation of this simple database of chemical structures is available from the examples directory of the source code distribution.

.. note::
    Loading the entire collection of ChEMBLdb compounds may take some time and the resulting file will require several GB of disk space. The `create_chembldb.py` script supports a 
    command line option that may help limiting the number of imported compounds during tests.

Substructure queries
--------------------

A search for substructures could be performed with a query like the following::

    SELECT COUNT(*) FROM chembl WHERE mol_is_substruct(molecule, mol_from_smiles('c1ccnnc1'));

but this kind of simple query would sequentially check every single molecule in the `chembl` table, resulting very inefficient. 

Support for custom indexes in SQLite is a bit different than other database engines. The data structure of a custom index is in fact wrapped behind the implementation of a "virtual table", an object that exposes the interface of a regular SQL table, but whose implementation can be customized.

ChemicaLite uses this virtual table mechanism to support indexing binary fingerprints in an RD-tree data structure, and this way improve the performances of substructure and similarity queries.

An RD-tree virtual table for substructure queries is created with a statement like the following::

    connection.execute("CREATE VIRTUAL TABLE str_idx_chembl_molecule " +
                       "USING rdtree(id, fp bits(2048))")

And this index table is then filled with the structural fingerprint data generated from the `chembl` table::

    with connection:
        connection.execute( 
            "INSERT INTO str_idx_chembl_molecule(id, fp) " + 
            "SELECT id, mol_pattern_bfp(molecule, 2048) FROM chembl " + 
            "WHERE molecule IS NOT NULL")

The performances of the substructure query above can this way strongly improve if the index table is joined, and a substructure constraint is specified using an `rdtree_subset` match object::

    SELECT COUNT(*) FROM chembl, str_idx_chembl_molecule AS idx WHERE
        chembl.id = idx.id AND 
        mol_is_substruct(chembl.molecule, mol_from_smiles('c1ccnnc1')) AND
        idx.id MATCH rdtree_subset(mol_pattern_bfp(mol_from_smiles('c1ccnnc1'), 2048));

A python script executing this second query is available from the `examples` directory of the source code distribution::

    # returns the number of structures containing the query fragment.
    $ ./match_count.py /path/to/chembldb.sql c1ccnnc1

And here are some example queries::

    $ ./match_count.py chembldb.sql c1cccc2c1nncc2
    searching for substructure: c1cccc2c1nncc2
    Found 525 matching structures in 0.226271390914917 seconds

    $ ./match_count.py chembldb.sql c1ccnc2c1nccn2
    searching for substructure: c1ccnc2c1nccn2
    Found 1143 matching structures in 0.3587167263031006 seconds

    $ ./match_count.py chembldb.sql Nc1ncnc\(N\)n1
    searching for substructure: Nc1ncnc(N)n1
    Found 8197 matching structures in 0.8730080127716064 seconds
    
    $ ./match_count.py chembldb.sql c1scnn1
    searching for substructure: c1scnn1
    Found 17918 matching structures in 1.2525584697723389 seconds
    
    $ ./match_count.py chembldb.sql c1cccc2c1ncs2
    searching for substructure: c1cccc2c1ncs2
    Found 23277 matching structures in 1.7844812870025635 seconds
    
    $ ./match_count.py chembldb.sql c1cccc2c1CNCCN2
    searching for substructure: c1cccc2c1CNCCN2
    Found 1973 matching structures in 2.547306776046753 seconds

*(Execution times are only provided for reference and may vary depending on the available computational resources)*.   

A second script is available in the examples directory, and it illustrates how to return only the first results (sometimes useful for queries that return a large number of matches)::

    $ ./substructure_search.py chembldb.sql c1cccc2c1CNCCN2
    searching for substructure: c1cccc2c1CNCCN2
    CHEMBL7892 CC(=O)Nc1ccc2c(c1)C(=O)N(C(C(=O)NC1CCCCC1)c1ccc([N+](=O)[O-])cc1)[C@@H](c1ccccc1)C(=O)N2
    CHEMBL415394 CC(C)[C@H](NC(=O)[C@H](CCCN=C(N)N)NC(=O)[C@@H](N)CC(=O)O)C(=O)N[C@@H](Cc1ccc(O)cc1)C(=O)Nc1ccc2c(c1)CN(CC(=O)N[C@@H](Cc1ccccc1)C(=O)O)C(=O)[C@H](Cc1c[nH]cn1)N2
    CHEMBL8003 O=S(=O)(c1cc(Cl)ccc1Cl)N1Cc2ccccc2N(Cc2c[nH]cn2)C(CCc2ccccc2)C1
    [...]
    CHEMBL53987 Cc1cccc(NCCNC(=O)c2ccc3c(c2)CN(C)C(=O)[C@H](CC(=O)O)N3)n1
    CHEMBL53985 CN1Cc2cc(C(=O)NCc3c[nH]cn3)ccc2N[C@@H](CC(=O)O)C1=O
    CHEMBL57915 CC(C)C[C@H]1C(=O)N2c3ccccc3[C@@](O)(C[C@@H]3NC(=O)c4ccccc4N4C(=O)c5ccccc5NC34)[C@H]2N1C(=O)CCC(=O)[O-].[Na+]
    CHEMBL50075 CN1Cc2cc(C(=O)NCCNc3ccccc3)ccc2N[C@@H](CC(=O)O)C1=O
    CHEMBL50257 CN1Cc2cc(C(=O)NCCc3cccc(N)n3)ccc2N[C@@H](CC(=O)O)C1=O
    Found 25 matches in 0.08957481384277344 seconds

Similarity queries
------------------

In the same way, binary fingerprint data for similarity queries is conveniently stored and indexed into RD-tree virtual tables, as illustrated by the following statements::

    # create a virtual table to be filled with morgan bfp data
    connection.execute("CREATE VIRTUAL TABLE morgan_idx_chembl_molecule " +
                "USING rdtree(id, fp bits(1024))");

    # compute and insert the fingerprints
    with connection:
        connection.execute( 
            "INSERT INTO morgan_idx_chembl_molecule(id, fp) " + 
            "SELECT id, mol_morgan_bfp(molecule, 2, 1024) FROM chembl " + 
            "WHERE molecule IS NOT NULL")

A search for similar structures is therefore based on filtering this new virtual table. The following statement would for example return the number of compounds with a Tanimoto similarity greater than or equal to the threshold value (see also the `tanimoto_count.py` script for a complete example)::

    count = connection.execute(
        "SELECT count(*) FROM "
        "morgan_idx_chembl_molecule as idx WHERE "
        "idx.id match rdtree_tanimoto(mol_morgan_bfp(mol_from_smiles(?), 2, 1024), ?)",
        (target, threshold)).fetchall()[0][0]

A sorted list of SMILES strings identifying the most similar compounds is instead for example returned by the following query::

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

These last two examples show the output produced by the `tanimoto_search.py` script, which is based on the previous query::

    $ ./tanimoto_search.py /path/to/chembldb.sql "Cc1ccc2nc(-c3ccc(NC(C4N(C(c5cccs5)=O)CCC4)=O)cc3)sc2c1" 0.5
    searching for target: Cc1ccc2nc(-c3ccc(NC(C4N(C(c5cccs5)=O)CCC4)=O)cc3)sc2c1
    CHEMBL467428 Cc1ccc2nc(-c3ccc(NC(=O)C4CCN(C(=O)c5cccs5)CC4)cc3)sc2c1 0.7611940298507462
    CHEMBL461435 Cc1ccc2nc(-c3ccc(NC(=O)C4CCCN(S(=O)(=O)c5cccs5)C4)cc3)sc2c1 0.6486486486486487
    CHEMBL460340 Cc1ccc2nc(-c3ccc(NC(=O)C4CCN(S(=O)(=O)c5cccs5)CC4)cc3)sc2c1 0.6301369863013698
    [...]
    CHEMBL218058 Cc1ccc2nc(-c3ccc(NC(=O)Nc4ccc(Cl)cc4)cc3)sc2c1 0.5
    CHEMBL1317763 Cc1cc(C)c(NC(=O)CNC(=O)[C@@H]2CCCN2C(=O)c2cccs2)c(C)c1 0.5
    Found 54 matches in 0.6459760665893555 seconds

::

    $ ./tanimoto_search.py /path/to/chemicalite.so /path/to/chembldb.sql "Cc1ccc2nc(N(C)CC(=O)O)sc2c1" 0.5
    CHEMBL394654 Cc1ccc2nc(N(C)CCN(C)c3nc4ccc(C)cc4s3)sc2c1 0.6923076923076923
    CHEMBL3928717 CN(CC(=O)O)c1nc2ccc([N+](=O)[O-])cc2s1 0.6739130434782609
    CHEMBL491074 CN(CC(=O)O)c1nc2cc([N+](=O)[O-])ccc2s1 0.5833333333333334
    [...]
    CHEMBL1617545 Cc1ccc2nc(N(CCCN(C)C)C(=O)CCc3ccccc3)sc2c1 0.5087719298245614
    CHEMBL1351385 Cc1ccc2nc(N(CCCN(C)C)C(=O)CCc3ccccc3)sc2c1.Cl 0.5
    CHEMBL1418054 Cc1ccc2nc(N(CCN(C)C)C(=O)c3ccc4ccccc4c3)sc2c1.Cl 0.5
    Found 12 matches in 1.2354457378387451 seconds