export CPATH=$PREFIX/include
python setup.py build --enable=load_extension
python setup.py install #test

