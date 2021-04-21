#ifndef CHEMICALITE_RDTREE_STRATEGY_INCLUDED
#define CHEMICALITE_RDTREE_STRATEGY_INCLUDED

#include "rdtree_vtab.hpp"

#if 0
class RDtreeStrategy {
public:
  RDtreeStrategy(RDtreeVtab *vtab) : vtab_(vtab) {}
  virtual ~RDtreeStrategy() {}

  virtual int assign_items(
    RDtreeItem *items, int num_items,
	RDtreeNode *left, RDtreeNode *right,
	RDtreeItem *left_bounds, RDtreeItem *right_bounds) = 0;

  /*
  ** This function implements the chooseLeaf algorithm from Gutman[84].
  ** ChooseSubTree in r*tree terminology.
  */
  virtual int choose_leaf(RDtreeItem *item, int height, RDtreeNode **leaf) = 0;

protected:
  RDtreeVtab * vtab_;    
};
#endif

class RDtreeGenericStrategy : public RDtreeVtab {
public:

  virtual int assign_items(
    RDtreeItem *items, int num_items,
	RDtreeNode *left, RDtreeNode *right,
	RDtreeItem *left_bounds, RDtreeItem *right_bounds);

  /*
  ** Pick the two most dissimilar fingerprints.
  */
  virtual void pick_seeds(
    RDtreeItem *items, int num_items, int *left_seed_idx, int *right_seed_idx);

  /*
  ** Pick the next item to be inserted into one of the two subsets. Select the
  ** one associated to a strongest "preference" for one of the two.
  */
  virtual void pick_next(
    RDtreeItem *items, int num_items, int *used,
    RDtreeItem *left_seed, RDtreeItem *right_seed,
	RDtreeItem **next_item, int *prefer_right);

  virtual int choose_leaf(RDtreeItem *item, int height, RDtreeNode **leaf);
};

class RDtreeSubsetStrategy : public RDtreeGenericStrategy {
public:

  /* at this time - after some back and forth - the node-splitting strategy
  ** "optimized" for subset queries, is just the same as the generic one
  */

  /*
  virtual void pick_seeds(
    RDtreeItem *items, int num_items, int *left_seed_idx, int *right_seed_idx);

  virtual void pick_next(
    RDtreeItem *items, int num_items, int *used,
    RDtreeItem *left_seed, RDtreeItem *right_seed,
	RDtreeItem **next_item, int *prefer_right);
  */

  virtual int choose_leaf(RDtreeItem *item, int height, RDtreeNode **leaf);
};

class RDtreeSimilarityStrategy : public RDtreeGenericStrategy {
public:
  virtual void pick_seeds(
    RDtreeItem *items, int num_items, int *left_seed_idx, int *right_seed_idx);

  virtual void pick_next(
    RDtreeItem *items, int num_items, int *used,
    RDtreeItem *left_seed, RDtreeItem *right_seed,
	RDtreeItem **next_item, int *prefer_right);

  virtual int choose_leaf(RDtreeItem *item, int height, RDtreeNode **leaf);
};

#endif
