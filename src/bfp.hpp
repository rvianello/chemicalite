#ifndef CHEMICALITE_BFP_INCLUDED
#define CHEMICALITE_BFP_INCLUDED
#include <string>

Blob bfp_to_blob(const std::string &, int *);
std::string blob_to_bfp(const Blob &, int *);

std::string arg_to_bfp(sqlite3_value *, int *);

#endif