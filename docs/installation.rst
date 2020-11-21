Installation
============

Some experimental conda packages are currently available for the Linux and OSX operating systems.

.. note::
  These packages are provided in the hope that they may make things a little bit simpler for users who may be curious to explore how the extension works. Any use in a productive environment is not at this time encouraged.

.. note::
  These packages were built using a conda-forge Miniforge distribution, and require RDKit 2020.09.1. The compatibility and interoperability with the Anaconda  distribution, or with other packages built on top of Anaconda, wasn't tested and it's not assumed.

To install the ChemicaLite extension using conda, just specify the `chemicalite/label/dev` channel, either on the conda command line, or in your `.condarc` file.

For example::

    $ conda install -c chemicalite/label/dev chemicalite

 

