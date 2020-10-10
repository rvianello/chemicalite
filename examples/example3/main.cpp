#include <cstdlib>
#include <iostream>
#include <fstream>

#include <sqlite3.h>

sqlite3 * open_database(const char * filename)
{
  sqlite3 * db;
  if (sqlite3_open(filename, &db) != SQLITE_OK) {
    std::cerr << "An error occurred while opening the db file: "
	      << filename << std::endl;
  }
  return db;
}


void load_chemicalite(sqlite3 * db)
{
  char * errmsg = 0;
  if (sqlite3_enable_load_extension(db, 1) != SQLITE_OK) {
    std::cerr << "Could not enable loading extensions" << std::endl;
  }
  else if (sqlite3_load_extension(db, "chemicalite", 0, &errmsg) != SQLITE_OK) {
    std::cerr << "An error occurred while loading chemicalite: "
	      << errmsg << std::endl;
    sqlite3_free(errmsg);
  }
  else if (sqlite3_enable_load_extension(db, 0) != SQLITE_OK) {
    std::cerr << "Could not enable loading extensions" << std::endl;
  }
}


void extend_schema(sqlite3 * db)
{
  char * errmsg = 0;
  if (sqlite3_exec(db,
 		   "CREATE VIRTUAL TABLE morgan "
		   "USING rdtree(id, bfp bytes(64),"
		   " OPT_FOR_SIMILARITY_QUERIES)",  
		   NULL,  /* Callback function */
		   0,     /* 1st argument to callback */
		   &errmsg) != SQLITE_OK) {
    std::cerr << "An error occurred while extending the db schema: "
	      << errmsg << std::endl;
    sqlite3_free(errmsg);
  }
}


void insert_fingerprints(sqlite3 * db)
{
  char * errmsg = 0;
  if (sqlite3_exec(db,
 		   "INSERT INTO morgan(id, bfp) "
		   "SELECT id, mol_morgan_bfp(molecule, 2) FROM compounds",  
		   NULL,  /* Callback function */
		   0,     /* 1st argument to callback */
		   &errmsg) != SQLITE_OK) {
    std::cerr << "An error occurred while inserting the fingerprings data: "
	      << errmsg << std::endl;
    sqlite3_free(errmsg);
  }
}


void close_database(sqlite3 * db)
{
  if (sqlite3_close(db) != SQLITE_OK) {
    std::cerr << "An error occurred while closing the db" << std::endl;
  }
}


int main(int, char * argv[])
{
  const char * db_path = argv[1];
  
  sqlite3 * db = open_database(db_path);

  load_chemicalite(db);
  extend_schema(db);
  insert_fingerprints(db);
  close_database(db);
}
