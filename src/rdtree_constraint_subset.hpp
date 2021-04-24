#ifndef CHEMICALITE_RDTREE_CONSTRAINT_SUBSET_INCLUDED
#define CHEMICALITE_RDTREE_CONSTRAINT_SUBSET_INCLUDED
#include "rdtree_constraint.hpp"
#include "utils.hpp"

/**
*** Subset (substructure) match operator
**/

class RDtreeSubset : public RDtreeConstraint {
public:
  virtual int initialize();
  virtual int test_internal(const RDtreeItem &, bool &);
  virtual int test_leaf(const RDtreeItem &, bool &);
  int test(const RDtreeItem &, bool &);

  int weight;
  Blob bfp;
};

#endif
