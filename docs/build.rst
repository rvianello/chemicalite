Building the extension
======================

Dependencies
------------

* SQLite (devel package too)
* RDKit

Support for loading extensions is often disabled in the `sqlite3` package that is provided by the python standard library. Using the extension from Python may therefore require a proper build of :doc:`pysqlite <pysqlite>` (python2.7 only), or or :doc:`APSW <apsw>`. APWS is also required for running the python tests.

Configure and build
-------------------

Default linux build::

    $ cd build/dir
    $ cmake path/to/chemicalite/dir -DRDKit_DIR=path/to/rdkit/lib/dir
    $ make
    $ LD_LIBRARY_PATH=. make test

Building with tests disabled::

    $ cmake path/to/chemicalite/dir \
        -DRDKit_DIR=path/to/rdkit/lib/dir -DCHEMICALITE_ENABLE_TESTS=OFF

Or only python tests disabled::

    $ cmake path/to/chemicalite/dir \
        -DRDKit_DIR=path/to/rdkit/lib/dir -DCHEMICALITE_ENABLE_PYTHON_TESTS=OFF
	
	
