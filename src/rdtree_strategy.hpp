#ifndef CHEMICALITE_RDTREE_STRATEGY_INCLUDED
#define CHEMICALITE_RDTREE_STRATEGY_INCLUDED

#include "rdtree_vtab.hpp"

class RDtreeGenericStrategy : public RDtreeVtab {
public:

  virtual int assign_items(
    RDtreeItem *items, int num_items,
	RDtreeNode *left, RDtreeNode *right,
	RDtreeItem *left_bounds, RDtreeItem *right_bounds);

#if 0
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
#endif

  /*
  ** This function implements the chooseLeaf algorithm from Gutman[84].
  ** ChooseSubTree in r*tree terminology.
  */
  virtual int choose_node(RDtreeItem *item, int height, RDtreeNode **leaf);
};

#endif
