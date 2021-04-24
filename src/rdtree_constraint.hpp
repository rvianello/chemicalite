#ifndef CHEMICALITE_RDTREE_CONSTRAINT_INCLUDED
#define CHEMICALITE_RDTREE_CONSTRAINT_INCLUDED
#include <memory>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

class RDtreeVtab;
class RDtreeItem;

/*
** A bitstring search constraint.
*/

class RDtreeConstraint {
public:
  static std::shared_ptr<RDtreeConstraint> deserialize(const uint8_t * data, int size, const RDtreeVtab *, int * rc);

  virtual int initialize() = 0;
  virtual int test_internal(const RDtreeItem &, bool &) = 0;
  virtual int test_leaf(const RDtreeItem &, bool &) = 0;
};


#endif
