#ifndef CHEMICALITE_RDTREE_VTAB_INCLUDED
#define CHEMICALITE_RDTREE_VTAB_INCLUDED
#include <string>
#include <unordered_map>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

class RDtreeNode;
class RDtreeItem;

class RDtreeVtab : public sqlite3_vtab {

public:
  static int create(
    sqlite3 *db, void */*paux*/, int argc, const char *const*argv, 
	sqlite3_vtab **pvtab, char **err);
  static int connect(
    sqlite3 *db, void */*paux*/, int argc, const char *const*argv, 
	sqlite3_vtab **pvtab, char **err);
  int disconnect();
  int destroy();
  int rowid(sqlite3_vtab_cursor *pVtabCursor, sqlite_int64 *pRowid);
  int update(int argc, sqlite3_value **argv, sqlite_int64 *pRowid);

private:
  static int init(
    sqlite3 *db, int argc, const char *const*argv, 
	sqlite3_vtab **pvtab, char **err, int is_create);
  void incref();
  void decref();
  int get_node_size(int is_create);
  int sql_init(int is_create);
  int delete_rowid(sqlite3_int64 rowid);
  int delete_item(RDtreeNode *node, int item, int height);
  int insert_item(RDtreeNode *node, RDtreeItem *item, int height);
  int remove_node(RDtreeNode *node, int height);
  int reinsert_node_content(RDtreeNode *node);
  int split_node(RDtreeNode *node, RDtreeItem *item, int height);
  int assign_items(RDtreeItem *items, int num_items,
		      RDtreeNode *left, RDtreeNode *right,
		      RDtreeItem *leftbounds, RDtreeItem *rightbounds);
  void pick_seeds(RDtreeItem *items, int num_items, int *leftseed, int *rightseed);
  void pick_seeds_subset(RDtreeItem *items, int num_items, int *leftseed, int *rightseed);
  void pick_seeds_similarity(RDtreeItem *items, int num_items, int *leftseed, int *rightseed);
  void pick_seeds_generic(RDtreeItem *items, int num_items, int *leftseed, int *rightseed);
  void pick_next(
        RDtreeItem *aItem, int nItem, int *aiUsed,
        RDtreeItem *pLeftSeed, RDtreeItem *pRightSeed,
		    RDtreeItem *pLeftBounds, RDtreeItem *pRightBounds,
		    RDtreeItem **ppNext, int *pPreferRight);
  void pick_next_subset(
        RDtreeItem *aItem, int nItem, int *aiUsed,
        RDtreeItem *pLeftSeed, RDtreeItem *pRightSeed,
		    RDtreeItem *pLeftBounds, RDtreeItem *pRightBounds,
		    RDtreeItem **ppNext, int *pPreferRight);
  void pick_next_similarity(
        RDtreeItem *aItem, int nItem, int *aiUsed,
        RDtreeItem *pLeftSeed, RDtreeItem *pRightSeed,
		    RDtreeItem *pLeftBounds, RDtreeItem *pRightBounds,
		    RDtreeItem **ppNext, int *pPreferRight);
  void pick_next_generic(
        RDtreeItem *aItem, int nItem, int *aiUsed,
        RDtreeItem *pLeftSeed, RDtreeItem *pRightSeed,
		    RDtreeItem *pLeftBounds, RDtreeItem *pRightBounds,
		    RDtreeItem **ppNext, int *pPreferRight);
  int choose_leaf_subset(RDtreeItem *item, int height, RDtreeNode **leaf);
  int choose_leaf_similarity(RDtreeItem *item, int height, RDtreeNode **leaf);
  int choose_leaf_generic(RDtreeItem *item, int height, RDtreeNode **leaf);
  int choose_leaf(RDtreeItem *item, int height, RDtreeNode **leaf);
  int update_mapping(sqlite3_int64 rowid, RDtreeNode *node, int height);
  int rowid_write(sqlite3_int64 rowid, sqlite3_int64 nodeid);
  int parent_write(sqlite3_int64 nodeid, sqlite3_int64 parentid);
  int adjust_tree(RDtreeNode *node, RDtreeItem *item);
  int fix_node_bounds(RDtreeNode *node);
  int fix_leaf_parent(RDtreeNode *leaf);
  int new_rowid(sqlite3_int64 *rowid);

  RDtreeNode * node_new(RDtreeNode *parent);
  void node_zero(RDtreeNode *node);

  int node_acquire(
    sqlite3_int64 nodeid, RDtreeNode *parent, RDtreeNode **acquired);
  int find_leaf_node(sqlite3_int64 rowid, RDtreeNode **leaf);
  int node_rowid_index(RDtreeNode *node, sqlite3_int64 rowid, int *index);
  int node_parent_index(RDtreeNode *node, int *index);
  sqlite3_int64 node_get_rowid(RDtreeNode *node, int item);
  uint8_t *node_get_bfp(RDtreeNode *node, int item);
  int node_get_min_weight(RDtreeNode *node, int item);
  int node_get_max_weight(RDtreeNode *node, int item);
  void node_get_item(RDtreeNode *node, int idx, RDtreeItem *item);
  int node_insert_item(RDtreeNode *node, RDtreeItem *item);
  void node_delete_item(RDtreeNode *node, int item);
  void node_overwrite_item(RDtreeNode *node, RDtreeItem *item, int idx);
  void node_incref(RDtreeNode *);
  int node_decref(RDtreeNode *);
  int node_write(RDtreeNode *node);
  int node_release(RDtreeNode *node);
  int node_minsize() {return node_capacity/3;}

  void node_hash_insert(RDtreeNode * node);
  RDtreeNode * node_hash_lookup(sqlite3_int64 nodeid);
  void node_hash_remove(RDtreeNode * node);

  double item_weight_distance(RDtreeItem *a, RDtreeItem *b);
  int item_weight(RDtreeItem *item);
  int item_contains(RDtreeItem *a, RDtreeItem *b);
  int item_growth(RDtreeItem *base, RDtreeItem *added);
  void item_extend_bounds(RDtreeItem *base, RDtreeItem *added);

  int increment_bitfreq(uint8_t *bfp);
  int decrement_bitfreq(uint8_t *bfp);
  int increment_weightfreq(int weight);
  int decrement_weightfreq(int weight);

  sqlite3 *db;                 /* Host database connection */
  unsigned int flags;          /* Configuration flags */
  int bfp_size;                /* Size (bytes) of the binary fingerprint */
  int bytes_per_item;          /* Bytes consumed per item */
  int node_size;               /* Size (bytes) of each node in the node table */
  int node_capacity;           /* Size (items) of each node */
  int depth;                   /* Current depth of the rd-tree structure */
  std::string db_name;         /* Name of database containing rd-tree table */
  std::string table_name;      /* Name of rd-tree table */ 
  std::unordered_map<sqlite3_int64, RDtreeNode *> node_hash; /* Hash table of in-memory nodes. */ 
  int n_ref;                   /* Current number of users of this structure */

  /* List of nodes removed during a CondenseTree operation. List is
  ** linked together via the pointer normally used for hash chains -
  ** RDtreeNode.pNext. RDtreeNode.iNode stores the depth of the sub-tree 
  ** headed by the node (leaf nodes have RDtreeNode.iNode==0).
  */
  RDtreeNode *pDeleted;

  /* Statements to read/write/delete a record from xxx_node */
  sqlite3_stmt *pReadNode;
  sqlite3_stmt *pWriteNode;
  sqlite3_stmt *pDeleteNode;

  /* Statements to read/write/delete a record from xxx_rowid */
  sqlite3_stmt *pReadRowid;
  sqlite3_stmt *pWriteRowid;
  sqlite3_stmt *pDeleteRowid;

  /* Statements to read/write/delete a record from xxx_parent */
  sqlite3_stmt *pReadParent;
  sqlite3_stmt *pWriteParent;
  sqlite3_stmt *pDeleteParent;

  /* Statements to update the bit frequencies in xxx_bitfreq */
  sqlite3_stmt *pIncrementBitfreq;
  sqlite3_stmt *pDecrementBitfreq;

  /* Statements to update the weight frequencies in xxx_weightfreq */
  sqlite3_stmt *pIncrementWeightfreq;
  sqlite3_stmt *pDecrementWeightfreq;
};

#endif
