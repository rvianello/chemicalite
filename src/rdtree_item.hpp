#ifndef CHEMICALITE_RDTREE_ITEM_INCLUDED
#define CHEMICALITE_RDTREE_ITEM_INCLUDED
#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

/* 
** Structure to store a deserialized rd-tree record.
*/
class RDtreeItem {
public:
  sqlite3_int64 rowid;
  int min_weight;
  int max_weight;
  Blob bfp;
};

#endif