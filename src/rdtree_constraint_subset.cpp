#include <algorithm>

#include "rdtree_vtab.hpp"
#include "rdtree_constraint_subset.hpp"
#include "rdtree_item.hpp"
#include "bfp_ops.hpp"

std::shared_ptr<RDtreeConstraint> RDtreeSubset::create(const uint8_t * data, int size, const RDtreeVtab *vtab, int * rc)
{
  std::shared_ptr<RDtreeConstraint> result;

  if (size != vtab->bfp_bytes) {
    *rc = SQLITE_MISMATCH;
  }
  else {
    result = std::shared_ptr<RDtreeConstraint>(new RDtreeSubset(data, size));
  }

  return result;
}

RDtreeSubset::RDtreeSubset(const uint8_t * data, int size)
  : bfp(data, data+size)
{
  weight = bfp_op_weight(size, data);
}

int RDtreeSubset::initialize() const {return SQLITE_OK;}

int RDtreeSubset::test_internal(const RDtreeItem & item, bool & eof) const {return test(item, eof);}
int RDtreeSubset::test_leaf(const RDtreeItem & item, bool & eof) const {return test(item, eof);}

/*
** xTestInternal/xTestLeaf implementation for subset search/filtering
** same test is used for both internal and leaf nodes.
**
** If the item doesn't contain the constraint's bfp, then it's discarded (for
** internal nodes it means that if the bfp is not in the union of the child 
** nodes then it's in none of them). 
*/
int RDtreeSubset::test(const RDtreeItem & item, bool & eof) const
{
  if (item.max_weight < weight) {
    eof = true;
  }
  else {
    eof = !bfp_op_contains(item.bfp.size(), item.bfp.data(), bfp.data());
  }
  return SQLITE_OK;
}

Blob RDtreeSubset::do_serialize() const
{
  Blob result(4 + bfp.size());
  uint8_t * p = result.data();
  p += write_uint32(p, RDTREE_SUBSET_CONSTRAINT_MAGIC);
  std::copy(bfp.begin(), bfp.end(), p);
  return result;
}
