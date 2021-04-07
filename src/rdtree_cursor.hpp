#ifndef CHEMICALITE_RDTREE_CURSOR_INCLUDED
#define CHEMICALITE_RDTREE_CURSOR_INCLUDED
#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

class RDtreeVtab;
class RDtreeNode;

/*
** A bitstring search constraint.
*/
class RDtreeMatchOp;

class RDtreeConstraint {
public:
  /* Ok this part is a bit ugly and these data structures will benefit some
  ** redesign. FIXME
  */
  uint8_t aBfp[256]; // RDTREE_MAX_BITSTRING_SIZE];        /* Constraint value. */
  uint8_t aBfpFilter[256]; //RDTREE_MAX_BITSTRING_SIZE];  /* Subset constraint value */
  int iWeight;
  double dParam;
  RDtreeMatchOp *op;
};

class RDtreeMatchOp {
public:
  int (*xInitializeConstraint)(RDtreeVtab*, RDtreeConstraint*); 
  int (*xTestInternal)(RDtreeVtab*, RDtreeConstraint*, RDtreeItem*, bool*); 
  int (*xTestLeaf)(RDtreeVtab*, RDtreeConstraint*, RDtreeItem*, bool*);
};

/* 
** Structure to store a deserialized rd-tree record.
*/
class RDtreeCursor : public sqlite3_vtab_cursor {
public:
  RDtreeNode *node;                 /* Node cursor is currently pointing at */
  int item;                         /* Index of current item in pNode */
  int strategy;                     /* Copy of idxNum search parameter */
  int nConstraint;                  /* Number of entries in aConstraint */
  RDtreeConstraint *aConstraint;    /* Search constraints. */
};

#endif