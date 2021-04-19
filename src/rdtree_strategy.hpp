#ifndef CHEMICALITE_RDTREE_STRATEGY_INCLUDED
#define CHEMICALITE_RDTREE_STRATEGY_INCLUDED

class RDtreeVtab;
class RDtreeNode;
class RDtreeItem;

class RDtreeStrategy {
public:
  RDtreeStrategy(RDtreeVtab *vtab) : vtab_(vtab) {}
  virtual ~RDtreeStrategy() {}

  virtual int assign_items(
    RDtreeItem *items, int num_items,
	RDtreeNode *left, RDtreeNode *right,
	RDtreeItem *left_bounds, RDtreeItem *right_bounds) = 0;

  /* virtual int choose_leaf(RDtreeItem *item, int height, RDtreeNode **leaf) = 0; */

protected:
  RDtreeVtab * vtab_;    
};

class RDtreeStrategyGeneric : public RDtreeStrategy {
public:
  RDtreeStrategyGeneric(RDtreeVtab *vtab) : RDtreeStrategy(vtab) {}

  virtual int assign_items(
    RDtreeItem *items, int num_items,
	RDtreeNode *left, RDtreeNode *right,
	RDtreeItem *left_bounds, RDtreeItem *right_bounds);

  virtual void pick_seeds(
    RDtreeItem *items, int num_items, int *left_seed_idx, int *right_seed_idx);

  virtual void pick_next(
    RDtreeItem *items, int num_items, int *used,
    RDtreeItem *left_seed, RDtreeItem *right_seed,
	RDtreeItem **next_item, int *prefer_right);
};

class RDtreeStrategySubset : public RDtreeStrategyGeneric {
public:
  RDtreeStrategySubset(RDtreeVtab *vtab) : RDtreeStrategyGeneric(vtab) {}

  /* at this time - after some back and forth - the strategy "optimized"
  ** for subset queries, is just the same as the generic one
  */

  /*
  virtual void pick_seeds(
    RDtreeItem *items, int num_items, int *left_seed_idx, int *right_seed_idx);

  virtual void pick_next(
    RDtreeItem *items, int num_items, int *used,
    RDtreeItem *left_seed, RDtreeItem *right_seed,
	RDtreeItem **next_item, int *prefer_right);
  */
};

class RDtreeStrategySimilarity : public RDtreeStrategyGeneric {
public:
  RDtreeStrategySimilarity(RDtreeVtab *vtab) : RDtreeStrategyGeneric(vtab) {}

  virtual void pick_seeds(
    RDtreeItem *items, int num_items, int *left_seed_idx, int *right_seed_idx);

  virtual void pick_next(
    RDtreeItem *items, int num_items, int *used,
    RDtreeItem *left_seed, RDtreeItem *right_seed,
	RDtreeItem **next_item, int *prefer_right);
};

#endif
