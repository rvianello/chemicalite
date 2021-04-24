#ifndef CHEMICALITE_RDTREE_CONSTRAINT_SUBSET_INCLUDED
#define CHEMICALITE_RDTREE_CONSTRAINT_SUBSET_INCLUDED
#include "rdtree_constraint.hpp"
#include "utils.hpp"

/**
*** Subset (substructure) match operator
**/

class RDtreeSubset : public RDtreeConstraint {
public:
  static std::shared_ptr<RDtreeConstraint> create(const uint8_t * data, int size, const RDtreeVtab *, int * rc);

  RDtreeSubset(const uint8_t * data, int size);
  virtual int initialize() const;
  virtual int test_internal(const RDtreeItem &, bool &) const;
  virtual int test_leaf(const RDtreeItem &, bool &) const;
  int test(const RDtreeItem &, bool &) const;

  Blob bfp;
  int weight;

private:
  virtual Blob do_serialize() const;
};

#endif
