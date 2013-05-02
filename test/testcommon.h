#ifndef CHEMICALITE_C_TESTCOMMON_INCLUDED
#define CHEMICALITE_C_TESTCOMMON_INCLUDED
#include <sqlite3.h>

#include "../chemicalite.h"

int database_setup(const char * dbname, const char * extension, 
		   sqlite3 **pDb, char **pErrMsg);
int create_rdtree(sqlite3 *db, const char * name, int len, char **pErrMsg);
int insert_bitstring(sqlite3 *db, const char * name, 
		     int id, void *s, int len);
int insert_signature(sqlite3 *db, const char * name, 
		     int id, const char *smiles);
int select_integer(sqlite3 *db, const char * sql, int *pInt);

#endif
