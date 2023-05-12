#ifndef CHEMICALITE_RDTREE_CURSOR_INCLUDED
#define CHEMICALITE_RDTREE_CURSOR_INCLUDED
#include <memory>
#include <vector>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

class RDtreeNode;
class RDtreeConstraint;

/* 
** Structure to store a deserialized rd-tree record.
*/
class RDtreeCursor : public sqlite3_vtab_cursor {
public:
  using Constraints = std::vector<std::shared_ptr<RDtreeConstraint>>;
  
  RDtreeNode *node;                 /* Node cursor is currently pointing at */
  int item;                         /* Index of current item in pNode */
  int strategy;                     /* Copy of idxNum search parameter */
  Constraints constraints;          /* Search constraints. */
};

#endif