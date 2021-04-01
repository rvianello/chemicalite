#ifndef CHEMICALITE_RDTREE_CURSOR_INCLUDED
#define CHEMICALITE_RDTREE_CURSOR_INCLUDED
#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

class RDtreeNode;

/* 
** Structure to store a deserialized rd-tree record.
*/
class RDtreeCursor : public sqlite3_vtab_cursor {
public:
  RDtreeNode *node;                 /* Node cursor is currently pointing at */
  int item;                         /* Index of current item in pNode */
  int iStrategy;                    /* Copy of idxNum search parameter */
  int nConstraint;                  /* Number of entries in aConstraint */
  //RDtreeConstraint *aConstraint;    /* Search constraints. */
};

#endif