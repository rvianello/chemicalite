#ifndef CHEMICALITE_RDTREE_ITEM_INCLUDED
#define CHEMICALITE_RDTREE_ITEM_INCLUDED
#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"

/* 
** Structure to store a deserialized rd-tree record.
*/
class RDtreeItem {
public:
  explicit RDtreeItem(int sz);

  int weight() const; // CHECK - do we need this, or are there better ways?
  bool contains(const RDtreeItem &) const;
  int growth(const RDtreeItem &) const;
  void extend_bounds(const RDtreeItem &);

  static double weight_distance(const RDtreeItem &, const RDtreeItem &);

  sqlite3_int64 rowid;
  int min_weight;
  int max_weight;
  Blob bfp;
};

#endif