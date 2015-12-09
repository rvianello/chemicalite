pysqlite
========

In order to use ChemicaLite from python, the `sqlite3` package must be built with support for loading C extensions. Alternatively (this feature is often disabled for security reasons), pysqlite 2.5+ may be used. 

The custom version of pysqlite can be imported in place of the standard `sqlite3` module::

    >>> from pysqlite2 import dbapi2 as sqlite3
    >>> db = sqlite3.connect(':memory:')
    >>> db.enable_load_extension(True)
    >>> db.load_extension('/path/to/libchemicalite.so')
    >>> db.enable_load_extension(False)

A recipe for building pysqlite using conda is available from the chemicalite repository, instructions for building it from source code are included in the next section.

Build pysqlite from a source distribution
-----------------------------------------

Unpack the pysqlite sources in a temporary location:

::

    $ mkdir pysqlite && cd pysqlite
    $ wget http://pysqlite.googlecode.com/files/pysqlite-2.6.3.tar.gz
    $ tar zxvf pysqlite-2.6.3.tar.gz
    $ cd pysqlite-2.6.3

Edit the `setup.cfg` file and make sure that the line that disables extensions
loading is commented-out (also fix the include and library paths, in case your SQLite installation is not available in standard locations):

::

    [build_ext]
    #define=
    #include_dirs=/usr/local/include
    #library_dirs=/usr/local/lib
    libraries=sqlite3
    #define=SQLITE_OMIT_LOAD_EXTENSION

Build and install:

::

    $ python setup.py install

Test:

::

    $ cd <somewhere out of the build dir>
    $ python
    [...]
    >>> from pysqlite2 import test
    >>> test.test()
    [...]
    Ran 213 tests in 1.616s
    
    OK
    >>> 
