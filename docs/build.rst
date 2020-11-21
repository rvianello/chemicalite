Building the extension
======================

Dependencies
------------

* SQLite (devel package too)
* RDKit

Configure and build
-------------------

The supported build system uses `cmake`, and it requires RDKit 2020.09.1 or later.

.. note::
  The code was built successfully on some Linux and OSX systems. Building on other operating systems (e.g. Windows) might be possible, but it wasn't tested.

Default Linux build::

    $ cd build/dir
    $ cmake path/to/chemicalite/dir
    $ make
    $ LD_LIBRARY_PATH=$PWD/src make test

Building with tests disabled::

    $ cmake path/to/chemicalite/dir -DCHEMICALITE_ENABLE_TESTS=OFF

Or only python tests disabled::

    $ cmake path/to/chemicalite/dir -DCHEMICALITE_ENABLE_PYTHON_TESTS=OFF

.. note::

  Loading SQLite extensions from python code requires that this feature was enabled at build time in the `sqlite3` package. This is currently the case for the python packages available from conda-forge, and for the system python of some linux distributions (e.g. Fedora).
  
  Should python tests fail in your build environment because loading extensions was disabled in `sqlite3`, you can still consider using a different or customized database driver.

	
