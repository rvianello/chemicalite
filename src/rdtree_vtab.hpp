#ifndef CHEMICALITE_RDTREE_VTAB_INCLUDED
#define CHEMICALITE_RDTREE_VTAB_INCLUDED
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

class RDtreeNode;
class RDtreeItem;
class RDtreeCursor;

class RDtreeVtab : public sqlite3_vtab {
public:
  virtual ~RDtreeVtab() {}

  static int create(
    sqlite3 *db, void */*paux*/, int argc, const char *const*argv, 
	  sqlite3_vtab **pvtab, char **err);
  static int connect(
    sqlite3 *db, void */*paux*/, int argc, const char *const*argv, 
	  sqlite3_vtab **pvtab, char **err);
  int bestindex(sqlite3_index_info *idxinfo);
  int disconnect();
  int destroy();
  int open(sqlite3_vtab_cursor **cur);
  int close(sqlite3_vtab_cursor *cur);
  int filter(sqlite3_vtab_cursor *cur, 
			int idxnum, const char *idxstr,
			int argc, sqlite3_value **argv);
  int next(sqlite3_vtab_cursor *cur);
  int eof(sqlite3_vtab_cursor *cur);
  int column(sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int col);
  int rowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid);
  int update(int argc, sqlite3_value **argv, sqlite_int64 *pRowid);
  int rename(const char *newname);

  void incref();
  void decref();

  static int init(
    sqlite3 *db, int argc, const char *const*argv, 
	  sqlite3_vtab **pvtab, char **err, int is_create);

  int get_node_bytes(int is_create);
  int sql_init(int is_create);
  int delete_rowid(sqlite3_int64 rowid);
  int delete_item(RDtreeNode *node, int item, int height);
  int insert_item(RDtreeNode *node, RDtreeItem *item, int height);
  int remove_node(RDtreeNode *node, int height);
  int reinsert_node_content(RDtreeNode *node);
  int split_node(RDtreeNode *node, RDtreeItem *item, int height);
  int update_mapping(sqlite3_int64 rowid, RDtreeNode *node, int height);
  int adjust_tree(RDtreeNode *node, RDtreeItem *item);
  int fix_node_bounds(RDtreeNode *node);
  int fix_leaf_parent(RDtreeNode *leaf);
  int new_rowid(sqlite3_int64 *rowid);
  int test_item(RDtreeCursor *csr, int height, bool *is_eof);
  int descend_to_item(RDtreeCursor *csr, int height, bool *is_eof);

  /* Define the strategy with which full nodes are split */
  virtual int assign_items(
    RDtreeItem *items, int num_items,
  	RDtreeNode *left, RDtreeNode *right,
	  RDtreeItem *left_bounds, RDtreeItem *right_bounds) = 0;
  /*
  ** This function implements the chooseLeaf algorithm from Gutman[84].
  ** ChooseSubTree in r*tree terminology.
  */
  virtual int choose_leaf(RDtreeItem *item, int height, RDtreeNode **leaf) = 0;

  int node_acquire(
    sqlite3_int64 nodeid, RDtreeNode *parent, RDtreeNode **acquired);
  int find_leaf_node(sqlite3_int64 rowid, RDtreeNode **leaf);

  RDtreeNode * node_new(RDtreeNode *parent);
  void node_incref(RDtreeNode *);
  int node_decref(RDtreeNode *);
  int node_write(RDtreeNode *node);
  int node_release(RDtreeNode *node);
  int node_minsize() {return node_capacity/3;}

  void node_hash_insert(RDtreeNode * node);
  RDtreeNode * node_hash_lookup(sqlite3_int64 nodeid);
  void node_hash_remove(RDtreeNode * node);

  int increment_bitfreq(const uint8_t *bfp);
  int decrement_bitfreq(const uint8_t *bfp);
  int increment_weightfreq(int weight);
  int decrement_weightfreq(int weight);

  int rowid_write(sqlite3_int64 rowid, sqlite3_int64 nodeid);
  int parent_write(sqlite3_int64 nodeid, sqlite3_int64 parentid);

  sqlite3 *db;                 /* Host database connection */
  int bfp_bytes;               /* Size (bytes) of the binary fingerprint */
  int item_bytes;              /* Bytes consumed per item */
  int node_bytes;              /* Size (bytes) of each node in the node table */
  int node_capacity;           /* Size (items) of each node */
  int depth;                   /* Current depth of the rd-tree structure */
  std::string db_name;         /* Name of database containing rd-tree table */
  std::string table_name;      /* Name of rd-tree table */ 
  int n_ref;                   /* Current number of users of this structure */

  /* Hash table of in-memory nodes. */
  std::unordered_map<sqlite3_int64, RDtreeNode *> node_hash; 

  /* List of nodes removed during a CondenseTree operation. 
  ** RDtreeNode.node stores the depth of the sub-tree 
  ** headed by the node (leaf nodes have RDtreeNode.node==0).
  */
  std::stack<RDtreeNode *> removed_nodes;

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
