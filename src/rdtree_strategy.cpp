#include <cmath>
#include <vector>

#include "rdtree_strategy.hpp"
#include "rdtree_vtab.hpp"
#include "rdtree_node.hpp"
#include "rdtree_item.hpp"
#include "bfp_ops.hpp"


int RDtreeGenericStrategy::assign_items(
    RDtreeItem *items, int num_items,
	  RDtreeNode *left, RDtreeNode *right,
	  RDtreeItem *left_bounds, RDtreeItem *right_bounds)
{
  /* the first (num_items - 1) elements in items are the contents of the node we want to split.
  ** these items are assumed to be already ordered.
  ** the last element in the array is the new item. this new item needs to be inserted in the
  ** appropriate position.
  **
  ** as long as these preconditions are satisfied (FIXME: assert/verify the preconditions) the
  ** job should be relatively simple: the first half of the node's original content goes into the
  ** left node, the second half goes into the right node, and the new item needs to be inserted 
  ** at the correct position into one or the other node, so that at the end the output nodes are
  ** both correctly ordered.
  */
  
  // Note: maybe we could first determine the new item's position in the array, and save a few
  // comparisons, but I'm not sure it would be worth it considering the small size of the nodes.

  // Get a pointer to the new item, because we'll compare it to the items we are 
  // inserting until we find its location. Once inserted, this pointer is made null.
  RDtreeItem * new_item = &items[num_items - 1];

  // We are going to fill the left node with about a half of the items
  int left_insert_limit = num_items / 2;
  int left_insert_count = 0;

  // The index of the item we are about to insert into one of the new nodes
  int item_index = 0;

  // Fill the left node
  while (left_insert_count < left_insert_limit) {
    RDtreeItem *item = &items[item_index];
    // first check if it's time to insert the new item
    if (new_item && bfp_op_cmp(bfp_bytes, new_item->max.data(), item->max.data()) <= 0) {
      left->append_item(new_item);
      if (left_insert_count == 0) {
        *left_bounds = *new_item;
      }
      else {
        left_bounds->extend_bounds(*new_item);
      }
      ++left_insert_count;
      new_item = nullptr;
    }
    // then insert the item from the overfull node
    left->append_item(item);
    if (left_insert_count == 0) {
      *left_bounds = *item;
    }
    else {
      left_bounds->extend_bounds(*item);
    }
    ++left_insert_count;
    // move to the next item
    ++item_index;
  }

  // Now insert the remaining items (which may still include the new one)
  // into the right node.
  int right_insert_count = 0;
  int num_old_items = num_items - 1;
  while (item_index < num_old_items) {
    RDtreeItem *item = &items[item_index];
    // first check if it's time to insert the new item
    if (new_item && bfp_op_cmp(bfp_bytes, new_item->max.data(), item->max.data()) <= 0) {
      right->append_item(new_item);
      if (right_insert_count == 0) {
        *right_bounds = *new_item;
      }
      else {
        right_bounds->extend_bounds(*new_item);
      }
      ++right_insert_count;
      new_item = nullptr;
    }
    // then insert the item from the overfull node
    right->append_item(item);
    if (right_insert_count == 0) {
      *right_bounds = *item;
    }
    else {
      right_bounds->extend_bounds(*item);
    }
    ++right_insert_count;
    // move to the next item
    ++item_index;
  }

  // if the new item is still to be inserted, just append it
  // to the right node
  if (new_item) {
    right->append_item(new_item);
    right_bounds->extend_bounds(*new_item);
  }

  return SQLITE_OK;
}

int RDtreeGenericStrategy::choose_node(RDtreeItem *item, int height, RDtreeNode **leaf)
{
  RDtreeNode *node;
  int rc = node_acquire(1, 0, &node);

  for (int ii = 0; rc == SQLITE_OK && ii < (depth - height); ii++) {
    sqlite3_int64 selected_rowid = 0;

    int node_size = node->get_size();

    /* Select the child node based on the bfp ordering */
    for (int idx = 0; idx < node_size; idx++) {
      RDtreeItem curr_item(bfp_bytes);
      node->get_item(idx, &curr_item);
      selected_rowid = curr_item.rowid;

      if (bfp_op_cmp(bfp_bytes, item->max.data(), curr_item.max.data()) <= 0) {
        break;
      }
    }

    RDtreeNode *child;
    rc = node_acquire(selected_rowid, node, &child);
    node_decref(node);
    node = child;
  }

  *leaf = node;
  return rc;
}
