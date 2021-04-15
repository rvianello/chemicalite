#ifndef CHEMICALITE_RDTREE_NODE_INCLUDED
#define CHEMICALITE_RDTREE_NODE_INCLUDED
#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"

/* 
** An rd-tree structure node.
*/
class RDtreeNode {
public:
  int get_size() const;

  RDtreeNode *parent; /* Parent node in the tree */
  sqlite3_int64 nodeid;
  int n_ref;
  int is_dirty;
  Blob data;
  RDtreeNode *next;   /* Next node in this hash chain */
};

#endif