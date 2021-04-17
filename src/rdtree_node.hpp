#ifndef CHEMICALITE_RDTREE_NODE_INCLUDED
#define CHEMICALITE_RDTREE_NODE_INCLUDED
#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"

class RDtreeVtab;

/* 
** An rd-tree structure node.
*/
class RDtreeNode {
public:
  RDtreeNode(RDtreeVtab *vtab, RDtreeNode *parent);

  int get_size() const;
  int get_min_weight(int item) const;
  int get_max_weight(int item) const;
  const uint8_t * get_bfp(int item) const;
  sqlite3_int64 get_rowid(int item) const;

  RDtreeVtab *vtab;
  RDtreeNode *parent;
  sqlite3_int64 nodeid;
  int n_ref;
  bool dirty;
  Blob data;
  RDtreeNode *next;   /* Next node in this deleted chain */
};

#endif