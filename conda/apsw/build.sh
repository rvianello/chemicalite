python setup.py \
    fetch --all --version=3.28.0 \
    build --enable-all-extensions --enable=load_extension
python setup.py install --single-version-externally-managed --record=record.txt
