#ifndef CHEMICALITE_BITSTRING_INCLUDED
#define CHEMICALITE_BITSTRING_INCLUDED

int chemicalite_init_bitstring(sqlite3 *db);
int fetch_bfp_arg(sqlite3_value* arg, Bfp **ppBfp);

#endif
