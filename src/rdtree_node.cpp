#include <cassert>
#include <cstring>

#include "rdtree_node.hpp"
#include "rdtree_item.hpp"
#include "rdtree_vtab.hpp"
#include "bfp_ops.hpp"

/*
** The root node of an rd-tree always exists, even if the rd-tree table is
** empty. The nodeno of the root node is always 1. All other nodes in the
** table must be the same size as the root node. The content of each node
** is formatted as follows:
**
**   1. If the node is the root node (node 1), then the first 2 bytes
**      of the node contain the tree depth as a big-endian integer.
**      For non-root nodes, the first 2 bytes are left unused.
**
**   2. The next 2 bytes contain the number of entries currently 
**      stored in the node.
**
**   3. The remainder of the node contains the node entries. Each entry
**      consists of a single 64-bits integer followed by a binary fingerprint. 
**      For leaf nodes the integer is the rowid of a record. For internal
**      nodes it is the node number of a child page.
*/

RDtreeNode::RDtreeNode(RDtreeVtab *vtab_, RDtreeNode *parent_)
  : vtab(vtab_), parent(parent_), nodeid(0), n_ref(1), dirty(false),
    data(vtab_->node_bytes, 0)
{
}

int RDtreeNode::get_depth() const
{
  // This is only meaningful if this is the root node
  assert(nodeid == 1);
  return read_uint16(data.data());
}

int RDtreeNode::get_size() const
{
  return read_uint16(&data.data()[2]);
}

/*
** Clear the content of node p (set all bytes to 0x00).
*/
void RDtreeNode::zero()
{
  memset(&data.data()[2], 0, vtab->node_bytes-2);
  dirty = true;
}

/*
** Return the index of the parent's item containing a pointer to this node.
** If this is the root node, return -1.
*/
int RDtreeNode::get_index_in_parent(int *idx) const
{
  if (parent) {
    return parent->get_rowid_index(nodeid, idx);
  }
  *idx = -1;
  return SQLITE_OK;
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
** Return pointer to the max binary fingerprint associated with the given item and its
** descentants. If node is a leaf node, this is the same as the item's own fingerprint
*/
const uint8_t *RDtreeNode::get_max(int item) const
{
  assert(item < get_size());
  return &data.data()[
    4 + vtab->item_bytes*item + 8 /* rowid */ + 4 /* min/max weight */ + vtab->bfp_bytes /* bfp */];
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
  item->bfp.assign(bfp, bfp+vtab->bfp_bytes); // CHECK: or std::copy?
  const uint8_t *max = get_max(idx);
  item->max.assign(max, max+vtab->bfp_bytes);
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
  memcpy(p, item->bfp.data(), vtab->bfp_bytes); // FIXME std::copy
  p += vtab->bfp_bytes;
  memcpy(p, item->max.data(), vtab->bfp_bytes);
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
** Insert the contents of item. If the insert is successful, return SQLITE_OK.
**
** If there is not enough free space in pNode, return SQLITE_FULL.
*/
int RDtreeNode::insert_item(RDtreeItem *item)
{
  int node_size = get_size();  /* Current number of items in pNode */

  assert(node_size <= vtab->node_capacity);

  if (node_size < vtab->node_capacity) {
    // Insert the new item, while preserving the ordering within the
    // node.

    // 1 - determine the insert location (idx) for the new item
    int idx = 0;
    for (int idx=0; idx < node_size; ++idx) {
      RDtreeItem curr_item(vtab->bfp_bytes);
      get_item(idx, &curr_item);
      if (bfp_op_cmp(vtab->bfp_bytes, item->max.data(), curr_item.max.data()) <= 0) {
        break;
      }
    }

    // 2 - move the items from idx to node_size-1 one position forward
    uint8_t *src = &data.data()[4 + vtab->item_bytes*idx];
    uint8_t *dst = src + vtab->item_bytes; // one item forward
    int bytes = (node_size - idx) * vtab->item_bytes;
    memmove(dst, src, bytes);

    // 3 - overwrite the item at idx with the new one
    overwrite_item(idx, item);

    // update the node size
    write_uint16(&data.data()[2], node_size+1);
    dirty = true;
  } 

  return (node_size == vtab->node_capacity) ? SQLITE_FULL : SQLITE_OK;
}

int RDtreeNode::append_item(RDtreeItem *item)
{
  int node_size = get_size();  /* Current number of items in pNode */

  assert(node_size <= vtab->node_capacity);

  if (node_size < vtab->node_capacity) {
    overwrite_item(node_size, item);
    write_uint16(&data.data()[2], node_size+1);
    dirty = true;
  } 

  return (node_size == vtab->node_capacity) ? SQLITE_FULL : SQLITE_OK;
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

/*
** One of the items in node node is guaranteed to have a 64-bit 
** integer value equal to rowid. Return the index of this item.
*/
int RDtreeNode::get_rowid_index(sqlite3_int64 rowid, int *idx) const
{
  int node_size = get_size();
  for (int ii = 0; ii < node_size; ++ii) {
    if (get_rowid(ii) == rowid) {
      *idx = ii;
      return SQLITE_OK;
    }
  }
  return SQLITE_CORRUPT_VTAB;
}

