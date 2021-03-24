#ifndef CHEMICALITE_RDTREE_VTAB_INCLUDED
#define CHEMICALITE_RDTREE_VTAB_INCLUDED
#include <string>
#include <unordered_map>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

class RDtreeNode;

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
  int remove_node(RDtreeNode *node, int height);
  int reinsert_node_content(RDtreeNode *node);

  RDtreeNode * node_new(RDtreeNode *parent);
  void node_zero(RDtreeNode *node);

  int node_acquire(
    sqlite3_int64 node_id, RDtreeNode *parent, RDtreeNode **acquired);
  int find_leaf_node(sqlite3_int64 rowid, RDtreeNode **leaf);
  int node_rowid_index(RDtreeNode *node, sqlite3_int64 rowid, int *index);
  sqlite3_int64 node_get_rowid(RDtreeNode *node, int item);
  uint8_t *node_get_bfp(RDtreeNode *node, int item);
  int node_get_min_weight(RDtreeNode *node, int item);
  int node_get_max_weight(RDtreeNode *node, int item);
  void node_incref(RDtreeNode *);
  int node_decref(RDtreeNode *);
  int node_write(RDtreeNode *node);
  int node_release(RDtreeNode *node);

  void node_hash_insert(RDtreeNode * node);
  RDtreeNode * node_hash_lookup(sqlite3_int64 node_id);
  void node_hash_remove(RDtreeNode * node);

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
