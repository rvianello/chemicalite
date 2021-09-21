Pragma settings that may sometimes help
=======================================

page_size
---------

`page_size <https://sqlite.org/pragma.html#pragma_page_size>`_ settings impose an upper limit to the size of a node in the index tree, and therefore contribute to determining the maximum number of references to child nodes (fan-out). A larger page size, and therefore a larger fan-out, may help reduce the disk I/O and improve the performances of substructure or similarity queries.

This configuration parameter used to be more critically important a few years ago, when the default page size was almost always 1024 bytes, but beginning with SQLite v3.12.0 this value increased to 4096 bytes.

cache_size
----------

The default `cache_size <https://sqlite.org/pragma.html#pragma_cache_size>`_ is usually limited to 2000 KiB. Increasing this value may help keeping the data in memory and reduce the access to the disk.
