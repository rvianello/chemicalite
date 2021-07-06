#include "rdtree_item.hpp"
#include "bfp_ops.hpp"

RDtreeItem::RDtreeItem(int sz)
  : bfp(sz), max(sz)
{
}

double RDtreeItem::weight_distance(const RDtreeItem & a, const RDtreeItem & b)
{
  int d1 = abs(a.min_weight - b.min_weight);
  int d2 = abs(a.max_weight - b.max_weight);
  return (double) (d1 + d2);
  /* return (double) (d1 > d2) ? d1 : d2; */
}

int RDtreeItem::weight() const
{
  return bfp_op_weight(bfp.size(), bfp.data());
}

bool RDtreeItem::contains(const RDtreeItem & other) const
{
  return (
    min_weight <= other.min_weight &&
	  max_weight >= other.max_weight &&
	  bfp_op_contains(bfp.size(), bfp.data(), other.bfp.data()) &&
    bfp_op_cmp(max.size(), max.data(), other.max.data()) >= 0
    );
}

int RDtreeItem::growth(const RDtreeItem & added) const
{
  return bfp_op_growth(bfp.size(), bfp.data(), added.bfp.data());
}

void RDtreeItem::extend_bounds(const RDtreeItem & added)
{
  bfp_op_union(bfp.size(), bfp.data(), added.bfp.data());
  if (min_weight > added.min_weight) { min_weight = added.min_weight; }
  if (max_weight < added.max_weight) { max_weight = added.max_weight; }
  if (bfp_op_cmp(max.size(), max.data(), added.max.data()) < 0) {
    max = added.max;
  }
}
