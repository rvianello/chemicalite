#include <cmath>
#include <vector>

#include "rdtree_strategy.hpp"
#include "rdtree_vtab.hpp"
#include "rdtree_node.hpp"
#include "rdtree_item.hpp"
#include "bfp_ops.hpp"


int RDtreeStrategyGeneric::assign_items(
    RDtreeItem *items, int num_items,
	RDtreeNode *left, RDtreeNode *right,
	RDtreeItem *left_bounds, RDtreeItem *right_bounds)
{
  int left_seed_idx = 0;
  int right_seed_idx = 1;

  std::vector<int> used(num_items, 0);

  pick_seeds(items, num_items, &left_seed_idx, &right_seed_idx);

  *left_bounds = items[left_seed_idx];
  *right_bounds = items[right_seed_idx];
  left->insert_item(&items[left_seed_idx]);
  right->insert_item(&items[right_seed_idx]);
  used[left_seed_idx] = 1;
  used[right_seed_idx] = 1;

  for(int i = num_items - 2; i > 0; i--) {
    int prefer_right;
    RDtreeItem *next_item;
    pick_next(items, num_items, used.data(), &items[left_seed_idx], &items[right_seed_idx], &next_item, &prefer_right);

    if ((vtab_->node_minsize() - right->get_size() == i) || (prefer_right > 0 && (vtab_->node_minsize() - left->get_size() != i))) {
      right->insert_item(next_item);
      vtab_->item_extend_bounds(right_bounds, next_item);
    }
    else {
      left->insert_item(next_item);
      vtab_->item_extend_bounds(left_bounds, next_item);
    }
  }

  return SQLITE_OK;
}

void RDtreeStrategyGeneric::pick_seeds(
    RDtreeItem *items, int num_items, int *left_seed_idx, int *right_seed_idx)
{
  int left_idx = 0;
  int right_idx = 1;
  double max_distance = 0.;

  for (int ii = 0; ii < num_items; ii++) {
    for (int jj = ii + 1; jj < num_items; jj++) {
      double tanimoto 
        = bfp_op_tanimoto(vtab_->bfp_bytes, items[ii].bfp.data(), items[jj].bfp.data());
      double distance = 1. - tanimoto;

      if (distance > max_distance) {
        left_idx = ii;
        right_idx = jj;
        max_distance = distance;
      }
    }
  }

  *left_seed_idx = left_idx;
  *right_seed_idx = right_idx;
}

void RDtreeStrategyGeneric::pick_next(
    RDtreeItem *items, int num_items, int *used,
    RDtreeItem *left_seed, RDtreeItem *right_seed,
	RDtreeItem **next_item, int *prefer_right)
{
  int selected = -1;
  int right_is_closer = 0;
  double max_preference = -1.;

  for (int ii = 0; ii < num_items; ii++){
    if (used[ii] == 0) {
      double left 
        = 1. - bfp_op_tanimoto(vtab_->bfp_bytes, items[ii].bfp.data(), left_seed->bfp.data());
      double right 
        = 1. - bfp_op_tanimoto(vtab_->bfp_bytes, items[ii].bfp.data(), right_seed->bfp.data());
      double diff = left - right;
      double preference = 0.;
      if ((left + right) > 0.) { // CHECK don't want to divide by zero, but it's always >= 0.
        preference = fabs(diff)/(left + right);
      }
      if (selected < 0 || preference > max_preference) {
        max_preference = preference;
        selected = ii;
        right_is_closer = diff > 0.;
      }
    }
  }
  used[selected] = 1;
  *next_item = &items[selected];
  *prefer_right = right_is_closer;
}

int RDtreeStrategyGeneric::choose_leaf(RDtreeItem *item, int height, RDtreeNode **leaf)
{
  RDtreeNode *node;
  int rc = vtab_->node_acquire(1, 0, &node);

  for (int ii = 0; rc == SQLITE_OK && ii < (vtab_->depth - height); ii++) {
    sqlite3_int64 best = 0;

    int min_growth = 0;
    double min_distance = 0.;
    int min_weight = 0;

    int node_size = node->get_size();
    RDtreeNode *child;

    /* Select the child node which will be enlarged the least if pItem
    ** is inserted into it.
    */
    for (int idx = 0; idx < node_size; idx++) {
      RDtreeItem curr_item;
      node->get_item(idx, &curr_item);

      int growth = vtab_->item_growth(&curr_item, item);
      double distance = vtab_->item_weight_distance(&curr_item, item);
      int weight = vtab_->item_weight(&curr_item);

      if (idx == 0 || growth < min_growth ||
	        (growth == min_growth && distance < min_distance) ||
	        (growth == min_growth && distance == min_distance && weight < min_weight)) {
        min_growth = growth;
        min_distance = distance;
        min_weight = weight;
        best = curr_item.rowid;
      }
    }

    rc = vtab_->node_acquire(best, node, &child);
    vtab_->node_decref(node);
    node = child;
  }

  *leaf = node;
  return rc;
}

int RDtreeStrategySubset::choose_leaf(RDtreeItem *item, int height, RDtreeNode **leaf)
{
  RDtreeNode *node;
  int rc = vtab_->node_acquire(1, 0, &node);

  for (int ii = 0; rc == SQLITE_OK && ii < (vtab_->depth - height); ii++) {
    sqlite3_int64 best = 0;

    int min_growth = 0;
    int min_weight = 0;

    int node_size = node->get_size();
    RDtreeNode *child;

    /* Select the child node which will be enlarged the least if item
    ** is inserted into it. Resolve ties by choosing the entry with
    ** the smallest weight.
    */
    for (int idx = 0; idx < node_size; idx++) {
      RDtreeItem curr_item;
      int growth;
      int weight;
      node->get_item(idx, &curr_item);
      growth = vtab_->item_growth(&curr_item, item);
      weight = vtab_->item_weight(&curr_item);

      if (idx == 0 || growth < min_growth || (growth == min_growth && weight < min_weight) ) {
        min_growth = growth;
        min_weight = weight;
        best = curr_item.rowid;
      }
    }

    //sqlite3_free(aItem);
    rc = vtab_->node_acquire(best, node, &child);
    vtab_->node_decref(node);
    node = child;
  }

  *leaf = node;
  return rc;
}

void RDtreeStrategySimilarity::pick_seeds(
    RDtreeItem *items, int num_items, int *left_seed_idx, int *right_seed_idx)
{
  int left_idx = 0;
  int right_idx = 1;
  double max_distance = 0.;

  for (int ii = 0; ii < num_items; ii++) {
    for (int jj = ii + 1; jj < num_items; jj++) {

      double distance = vtab_->item_weight_distance(&items[ii], &items[jj]);
      
      if (distance > max_distance) {
        left_idx = ii;
        right_idx = jj;
        max_distance = distance;
      }
    }
  }

  *left_seed_idx = left_idx;
  *right_seed_idx = right_idx;
}

void RDtreeStrategySimilarity::pick_next(
    RDtreeItem *items, int num_items, int *used,
    RDtreeItem *left_seed, RDtreeItem *right_seed,
	RDtreeItem **next_item, int *prefer_right)
{
  int selected = -1;
  int right_is_closer = 0;
  double max_preference = -1.;
  int ii;

  for (ii = 0; ii < num_items; ii++) {
    if (used[ii] == 0) {
      double left = vtab_->item_weight_distance(&items[ii], left_seed);
      double right = vtab_->item_weight_distance(&items[ii], right_seed);
      double diff = left - right;
      double sum = left + right;
      double preference = fabs(diff);
      if (sum) {
        preference /= (sum);
      }
      if (selected < 0 || preference > max_preference) {
        max_preference = preference;
        selected = ii;
        right_is_closer = diff > 0.;
      }
    }
  }
  used[selected] = 1;
  *next_item = &items[selected];
  *prefer_right = right_is_closer;
}

int RDtreeStrategySimilarity::choose_leaf(RDtreeItem *item, int height, RDtreeNode **leaf)
{
  RDtreeNode *node;
  int rc = vtab_->node_acquire(1, 0, &node);

  for (int ii = 0; rc == SQLITE_OK && ii < (vtab_->depth - height); ii++) {
    sqlite3_int64 best = 0;

    int min_growth = 0;
    double min_distance = 0.;

    int node_size = node->get_size();
    RDtreeNode *child;

    /* Select the child node which will be enlarged the least if pItem
    ** is inserted into it.
    */
    for (int idx = 0; idx < node_size; idx++) {
      RDtreeItem curr_item;
      double distance;
      int growth;
      node->get_item(idx, &curr_item);
      distance = vtab_->item_weight_distance(&curr_item, item);
      growth = vtab_->item_growth(&curr_item, item);

      if (idx == 0 || distance < min_distance || (distance == min_distance && growth < min_growth) ) {
        min_distance = distance;
        min_growth = growth;
        best = curr_item.rowid;
      }
    }

    rc = vtab_->node_acquire(best, node, &child);
    vtab_->node_decref(node);
    node = child;
  }

  *leaf = node;
  return rc;
}
