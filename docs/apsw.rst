APSW
====

An interesting alternative to (or combination with) a custom :doc:`pysqlite <pysqlite>` build is represented by `APSW` (Another Python SQLite Wrapper).

Differently from `pysqlite`, `APSW` doesn't implement an API fully compliant with the Python DBAPI specifications (PEP 249), but instead it provides full access to the features offered by SQLite.

APSW is hosted at `<https://code.google.com/p/apsw/>`_ and the official documentation is also available `online <http://apidoc.apsw.googlecode.com/hg/index.html#>`_. 

A recipe for building `APSW` as a conda package is included in the chemicalite repository.

Build instructions
------------------

Download and unpack the sources to a temporary location::

    $ unzip apsw-3.7.13-r1.zip
    $ cd apsw-3.7.13-r1

Build, install and test::

    $ python setup.py build --enable=load_extension
    $ python setup.py install test

