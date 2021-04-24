#include "rdtree_constraint_subset.hpp"
#include "rdtree_item.hpp"
#include "bfp_ops.hpp"

int RDtreeSubset::initialize() {return SQLITE_OK;}

int RDtreeSubset::test_internal(const RDtreeItem & item, bool & eof) {return test(item, eof);}
int RDtreeSubset::test_leaf(const RDtreeItem & item, bool & eof) {return test(item, eof);}

/*
** xTestInternal/xTestLeaf implementation for subset search/filtering
** same test is used for both internal and leaf nodes.
**
** If the item doesn't contain the constraint's bfp, then it's discarded (for
** internal nodes it means that if the bfp is not in the union of the child 
** nodes then it's in none of them). 
*/
int RDtreeSubset::test(const RDtreeItem & item, bool & eof)
{
  if (item.max_weight < weight) {
    eof = true;
  }
  else {
    eof = !bfp_op_contains(item.bfp.size(), item.bfp.data(), bfp.data());
  }
  return SQLITE_OK;
}
