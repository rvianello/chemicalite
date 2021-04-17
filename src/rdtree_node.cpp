#include <cassert>
#include <cstring>

#include "rdtree_node.hpp"
#include "rdtree_item.hpp"
#include "rdtree_vtab.hpp"

RDtreeNode::RDtreeNode(RDtreeVtab *vtab_, RDtreeNode *parent_)
  : vtab(vtab_), parent(parent_), nodeid(0), n_ref(1), dirty(false),
    data(vtab_->node_bytes, 0), next(nullptr)
{
}

int RDtreeNode::get_size() const
{
  return read_uint16(&data.data()[2]);
}

/* Return the min weight computed on the fingerprints associated to this
** item. If node is a leaf node then this is the actual population count
** for the item's fingerprint. On internal nodes the min weight contributes
** to defining the cell bounds
*/
int RDtreeNode::get_min_weight(int item) const
{
  assert(item < get_size());
  return read_uint16(&data.data()[4 + vtab->item_bytes*item + 8]);
}

/* Return the max weight computed on the fingerprints associated to this
** item. If node is a leaf node then this is the actual population count
** for the item's fingerprint. On internal nodes the max weight contributes
** to defining the cell bounds
*/
int RDtreeNode::get_max_weight(int item) const
{
  assert(item < get_size());
  return read_uint16(&data.data()[4 + vtab->item_bytes*item + 8 /* rowid */ + 2 /* min weight */]);
}

/*
** Return pointer to the binary fingerprint associated with the given item of
** the given node. If node is a leaf node, this is a virtual table element.
** If it is an internal node, then the binary fingerprint defines the 
** bounds of a child node
*/
const uint8_t *RDtreeNode::get_bfp(int item) const
{
  assert(item < get_size());
  return &data.data()[4 + vtab->item_bytes*item + 8 /* rowid */ + 4 /* min/max weight */];
}

/*
** Deserialize item idx. Populate the structure pointed to by item with the results.
*/
void RDtreeNode::get_item(int idx, RDtreeItem *item) const
{
  item->rowid = get_rowid(idx);
  item->min_weight = get_min_weight(idx);
  item->max_weight = get_max_weight(idx);
  const uint8_t *bfp = get_bfp(idx);
  item->bfp.assign(bfp, bfp+vtab->bfp_bytes);
}

/*
** Overwrite item idx of node with the contents of item.
*/
void RDtreeNode::overwrite_item(int idx, RDtreeItem *item)
{
  uint8_t *p = &data.data()[4 + vtab->item_bytes*idx];
  p += write_uint64(p, item->rowid);
  p += write_uint16(p, item->min_weight);
  p += write_uint16(p, item->max_weight);
  memcpy(p, item->bfp.data(), vtab->bfp_bytes);
  dirty = true;
}

/*
** Remove the item with index iItem from node pNode.
*/
void RDtreeNode::delete_item(int idx)
{
  uint8_t *dst = &data.data()[4 + vtab->item_bytes*idx];
  uint8_t *src = dst + vtab->item_bytes;
  int bytes = (get_size() - idx - 1) * vtab->item_bytes;
  memmove(dst, src, bytes);
  write_uint16(&data.data()[2], get_size()-1);
  dirty = true;
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
