Building the extension
======================

Dependencies
------------

* `SQLite (devel package too) <https://sqlite.org>`_
* `RDKit <https://github.com/rdkit/rdkit>`_
* `Catch2 (to build the tests) <https://github.com/catchorg/Catch2>`_

Configure and build
-------------------

The supported build system uses `cmake`, and it requires RDKit 2020.09.1 or later.

.. note::
  The code was built successfully on some Linux and OSX systems. Building on other operating systems (e.g. Windows) is probably possible, but it wasn't tested.

Default Linux build::

    $ cd build/dir
    $ cmake path/to/chemicalite/dir
    $ make
    $ LD_LIBRARY_PATH=$PWD/src make test

Building with tests disabled::

    $ cmake path/to/chemicalite/dir -DCHEMICALITE_ENABLE_TESTS=OFF


	
