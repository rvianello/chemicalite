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

  int get_size() const;
  void zero();

  int get_depth() const;
  int get_min_weight(int item) const;
  int get_max_weight(int item) const;
  const uint8_t * get_bfp(int item) const;
  const uint8_t * get_max(int item) const;
  void get_item(int idx, RDtreeItem *item) const;
  void overwrite_item(int idx, RDtreeItem *item);
  void delete_item(int idx);
  int insert_item(RDtreeItem *item);
  int append_item(RDtreeItem *item);
  sqlite3_int64 get_rowid(int item) const;
  int get_rowid_index(sqlite3_int64 rowid, int *idx) const;
  int get_index_in_parent(int *idx) const;

  RDtreeVtab *vtab;
  RDtreeNode *parent;
  sqlite3_int64 nodeid;
  int n_ref;
  bool dirty;
  Blob data;
};

#endif