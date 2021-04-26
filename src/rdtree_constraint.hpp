#ifndef CHEMICALITE_RDTREE_CONSTRAINT_INCLUDED
#define CHEMICALITE_RDTREE_CONSTRAINT_INCLUDED
#include <memory>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"

class RDtreeVtab;
class RDtreeItem;

/*
** A bitstring search constraint.
*/

class RDtreeConstraint {
protected:
  static const uint32_t RDTREE_CONSTRAINT_MAGIC;
  static const uint32_t RDTREE_SUBSET_CONSTRAINT_MAGIC;
  static const uint32_t RDTREE_TANIMOTO_CONSTRAINT_MAGIC;

public:
  static std::shared_ptr<RDtreeConstraint> deserialize(const uint8_t * data, int size, const RDtreeVtab &, int * rc);

  virtual ~RDtreeConstraint() {}

  Blob serialize() const;
  virtual int initialize(const RDtreeVtab &) = 0;
  virtual int test_internal(const RDtreeItem &, bool &) const = 0;
  virtual int test_leaf(const RDtreeItem &, bool &) const = 0;

private:
  virtual Blob do_serialize() const = 0;
};


#endif
