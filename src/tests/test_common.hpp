#ifndef CHEMICALITE_TESTCOMMON_INCLUDED
#define CHEMICALITE_TESTCOMMON_INCLUDED
#include <sqlite3.h>

#include "../src/chemicalite.h"
#include "../src/utils.hpp"

int database_setup(const char * dbname, sqlite3 **pDb, char **pErrMsg);
int create_rdtree(sqlite3 *db, const char * name, int len, char **pErrMsg);
int create_rdtree_ex(sqlite3 *db, const char * name, int len, const char * opts,
		     char **pErrMsg);
int insert_bitstring(sqlite3 *db, const char * name, 
		     int id, void *s, int len);
int insert_signature(sqlite3 *db, const char * name, 
		     int id, const char *smiles);
int select_integer(sqlite3 *db, const char * sql, int *pInt);
int select_text(sqlite3 *db, const char * sql, const char **pTxt);

#endif
