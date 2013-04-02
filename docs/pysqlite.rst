pysqlite
========

In order to use ChemicaLite from python, pysqlite 2.5+ is required and 
must be compiled with support for dynamic loading of C extension (this feature is often disabled for security reasons). 

The custom version of pysqlite can then be used in place of the standard `sqlite3` module (this becomes particularly simple when operating in a sandboxed environment, e.g. virtualenv)::

    >>> from pysqlite2 import dbapi2 as sqlite3
    >>> db = sqlite3.connect(':memory:')
    >>> db.enable_load_extension(True)
    >>> db.load_extension('/path/to/libchemicalite.so')
    >>> db.enable_load_extension(False)

Build instructions
------------------

Unpack the pysqlite sources in a temporary location:

::

    $ mkdir pysqlite && cd pysqlite
    $ wget http://pysqlite.googlecode.com/files/pysqlite-2.6.3.tar.gz
    $ tar zxvf pysqlite-2.6.3.tar.gz
    $ cd pysqlite-2.6.3

Edit the `setup.cfg` file and comment-out the line that disables extensions
loading (the last one in this case; also fix the include and library paths, in
case your SQLite installation is not available in standard locations):

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
