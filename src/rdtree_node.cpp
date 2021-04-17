#include <cassert>

#include "rdtree_node.hpp"
#include "rdtree_vtab.hpp"

RDtreeNode::RDtreeNode(RDtreeVtab *vtab_, RDtreeNode *parent_)
  : vtab(vtab_), parent(parent_), nodeid(0), n_ref(1), is_dirty(0),
    data(vtab_->node_bytes, 0), next(nullptr)
{
}

int RDtreeNode::get_size() const
{
  return read_uint16(&data.data()[2]);
}

/*
** Return the 64-bit integer value associated with item item. If this
** is a leaf node, this is a rowid. If it is an internal node, then
** the 64-bit integer is a child page number.
*/
sqlite3_int64 RDtreeNode::get_rowid(int item) const
{
  assert(item < get_size());
  return read_uint64(&data.data()[4 + vtab->item_bytes*item]);
}
