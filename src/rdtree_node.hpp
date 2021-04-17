#ifndef CHEMICALITE_RDTREE_NODE_INCLUDED
#define CHEMICALITE_RDTREE_NODE_INCLUDED
#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"

class RDtreeVtab;
class RDtreeItem;

/* 
** An rd-tree structure node.
*/
class RDtreeNode {
public:
  RDtreeNode(RDtreeVtab *vtab, RDtreeNode *parent);

  sqlite3_int64 get_rowid(int item) const;
  int get_size() const;
  int get_min_weight(int item) const;
  int get_max_weight(int item) const;
  const uint8_t * get_bfp(int item) const;
  void get_item(int idx, RDtreeItem *item) const;
  void overwrite_item(int idx, RDtreeItem *item);
  void delete_item(int idx);
  int insert_item(RDtreeItem *item);

  RDtreeVtab *vtab;
  RDtreeNode *parent;
  sqlite3_int64 nodeid;
  int n_ref;
  bool dirty;
  Blob data;
  RDtreeNode *next;   /* Next node in this deleted chain */
};

#endif