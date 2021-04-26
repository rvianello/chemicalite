#include <algorithm>

#include "rdtree_constraint.hpp"
#include "rdtree_constraint_subset.hpp"
#include "rdtree_constraint_tanimoto.hpp"
#include "utils.hpp"

const uint32_t RDtreeConstraint::RDTREE_CONSTRAINT_MAGIC = 0x3daf12ab;
const uint32_t RDtreeConstraint::RDTREE_SUBSET_CONSTRAINT_MAGIC = 0x7c4f9902;
const uint32_t RDtreeConstraint::RDTREE_TANIMOTO_CONSTRAINT_MAGIC = 0xf8324b5e;

std::shared_ptr<RDtreeConstraint>
RDtreeConstraint::deserialize(const uint8_t *data, int size, const RDtreeVtab *vtab, int *rc)
{
  std::shared_ptr<RDtreeConstraint> result;

  if (size < 4+4) {
    *rc = SQLITE_ERROR;
    return result;
  }

  uint32_t magic = read_uint32(data);
  data += 4;

  if (magic != RDTREE_CONSTRAINT_MAGIC) {
    *rc = SQLITE_MISMATCH;
    return result;
  }

  uint32_t constraint_id = read_uint32(data);
  data += 4;

  switch (constraint_id) {
  case RDTREE_SUBSET_CONSTRAINT_MAGIC:
    result = RDtreeSubset::deserialize(data, size-8, vtab, rc);
    break;
  case RDTREE_TANIMOTO_CONSTRAINT_MAGIC:
    result = RDtreeTanimoto::deserialize(data, size-8, vtab, rc);
    break;
  default:
    *rc = SQLITE_ERROR;
  }

  return result;
}

Blob RDtreeConstraint::serialize() const
{
  Blob constraint = do_serialize();
  Blob result(4 + constraint.size());

  uint8_t * p = result.data();
  p += write_uint32(p, RDTREE_CONSTRAINT_MAGIC);

  std::copy(constraint.begin(), constraint.end(), p);
  return result;
}
