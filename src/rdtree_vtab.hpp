#ifndef CHEMICALITE_RDTREE_VTAB_INCLUDED
#define CHEMICALITE_RDTREE_VTAB_INCLUDED
#include <string>

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

private:
  static int init(
    sqlite3 *db, int argc, const char *const*argv, 
	sqlite3_vtab **pvtab, char **err, int is_create);
  void incref();
  void decref();
  int get_node_size(int is_create);
  int sql_init(int is_create);

  sqlite3 *db;                 /* Host database connection */
  unsigned int flags;          /* Configuration flags */
  int bfp_size;                /* Size (bytes) of the binary fingerprint */
  int bytes_per_item;          /* Bytes consumed per item */
  int node_size;               /* Size (bytes) of each node in the node table */
  int node_capacity;           /* Size (items) of each node */
  int depth;                   /* Current depth of the rd-tree structure */
  std::string db_name;         /* Name of database containing rd-tree table */
  std::string table_name;      /* Name of rd-tree table */ 
  //static constexpr const int HASHSIZE = 128;
  //RDtreeNode *aHash[HASHSIZE]; /* Hash table of in-memory nodes. */ 
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
