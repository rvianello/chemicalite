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


void substructure_search(sqlite3 * db, const char * substructure)
{
  const char * sql =
    "SELECT count(*) FROM "
    "chembl, str_idx_chembl_molecule as idx WHERE "
    "chembl.id = idx.id AND "
    "mol_is_substruct(chembl.molecule, ?) AND "
    "idx.id match rdtree_subset(mol_bfp_signature(?))"
    ;

  sqlite3_stmt * stmt = 0;
  if (sqlite3_prepare_v2(db, sql, -1, & stmt, 0) != SQLITE_OK) {
    std::cerr << "Could not prepare sql statement" << std::endl;
  }
  
  // bind the query parameters
  if ((sqlite3_bind_text(stmt, 1, substructure, -1, SQLITE_STATIC)
       != SQLITE_OK) ||
      (sqlite3_bind_text(stmt, 2, substructure, -1, SQLITE_STATIC)
       != SQLITE_OK)) {
    std::cerr << "Couldn't bind the query parameters" << std::endl;
  }

  // execute the query
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    std::cerr << "Couldn't execute sql statement" << std::endl;
  }
  else {
    int match_count = sqlite3_column_int(stmt, 0);
    std::cout << "Number of matching structures: " << match_count << std::endl;
  }

  if (sqlite3_finalize(stmt) != SQLITE_OK)  {
    std::cerr << "Couldn't finalize statement" << std::endl;
  }  
}


void close_database(sqlite3 * db)
{
  if (sqlite3_close(db) != SQLITE_OK) {
    std::cerr << "An error occurred while closing the db" << std::endl;
  }
}


int main(int argc, char * argv[])
{
  const char * db_path = argv[1];
  const char * substructure = argv[2];
  
  sqlite3 * db = open_database(db_path);

  load_chemicalite(db);
  substructure_search(db, substructure);
  close_database(db);
}
