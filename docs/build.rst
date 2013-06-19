Building the extension
======================

Dependencies
------------

* SQLite (devel package too)
* RDKit

Running tests and using the extension from Python will also require a proper build of :doc:`pysqlite <pysqlite>`. In addition to pysqlite, you may also want to consider :doc:`APSW <apsw>`.

Configure and build
-------------------

::

    $ cd build/dir
    $ cmake path/to/chemicalite/dir -DRDKit_DIR=path/to/rdkit/lib/dir
    $ make
    $ make test


