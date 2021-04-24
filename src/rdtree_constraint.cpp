#include "rdtree_constraint.hpp"

std::shared_ptr<RDtreeConstraint>
RDtreeConstraint::deserialize(const uint8_t * /*data*/, int /*size*/, const RDtreeVtab *, int * rc)
{
  *rc = SQLITE_ERROR;
  return std::shared_ptr<RDtreeConstraint>();
}
