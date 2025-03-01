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

#if 0
  int weight() const; // CHECK - do we need this, or are there better ways?
#endif
  bool contains(const RDtreeItem &) const;
#if 0
  int growth(const RDtreeItem &) const;
#endif
  void extend_bounds(const RDtreeItem &);

#if 0
  static double weight_distance(const RDtreeItem &, const RDtreeItem &);
#endif

  sqlite3_int64 rowid;
  int min_weight;
  int max_weight;
  Blob bfp;
  Blob max;
};

#endif