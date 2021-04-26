#ifndef CHEMICALITE_RDTREE_CONSTRAINT_TANIMOTO_INCLUDED
#define CHEMICALITE_RDTREE_CONSTRAINT_TANIMOTO_INCLUDED
#include "rdtree_constraint.hpp"
#include "utils.hpp"

/**
*** Tanimoto similarity match operator
**/

class RDtreeTanimoto : public RDtreeConstraint {
public:
  static std::shared_ptr<RDtreeConstraint> deserialize(const uint8_t * data, int size, const RDtreeVtab *, int * rc);

  RDtreeTanimoto(const uint8_t * data, int size, double threshold);
  virtual int initialize(const RDtreeVtab &);
  virtual int test_internal(const RDtreeItem &, bool &) const;
  virtual int test_leaf(const RDtreeItem &, bool &) const;

  double threshold;
  Blob bfp;
  int weight;
  Blob bfp_filter;
  
private:
  virtual Blob do_serialize() const;
};

#endif
