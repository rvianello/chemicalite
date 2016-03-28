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


void tanimoto_search(sqlite3 * db, const char * target, double threshold)
{
  const char * sql =
    "SELECT c.chembl_id, c.smiles, "
    "bfp_tanimoto(mol_morgan_bfp(c.molecule, 2), mol_morgan_bfp(?, 2)) as t "
    "FROM "
    "chembl as c JOIN "
    "(SELECT id FROM morgan WHERE "
    "id match rdtree_tanimoto(mol_morgan_bfp(?, 2), ?)) as idx "
    "USING(id) ORDER BY t DESC"
    ;

  sqlite3_stmt * stmt = 0;
  if (sqlite3_prepare_v2(db, sql, -1, & stmt, 0) != SQLITE_OK) {
    std::cerr << "Could not prepare sql statement" << std::endl;
  }
  
  // bind the query parameters
  if ((sqlite3_bind_text(stmt, 1, target, -1, SQLITE_STATIC)
       != SQLITE_OK) ||
      (sqlite3_bind_text(stmt, 2, target, -1, SQLITE_STATIC)
       != SQLITE_OK) ||
      (sqlite3_bind_double(stmt, 3, threshold) != SQLITE_OK)) {
    std::cerr << "Couldn't bind the query parameters" << std::endl;
  }

  // execute the query
  int match_count = 0;
  int query_status = sqlite3_step(stmt);
  while (query_status == SQLITE_ROW) {
    ++match_count;
    query_status = sqlite3_step(stmt);
  }

  if (query_status != SQLITE_DONE) {
    std::cerr << "An error occurred while processing the similarity query" 
	      << std::endl;
  }
  else {
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
  const char * target = argv[2];
  
  sqlite3 * db = open_database(db_path);

  load_chemicalite(db);
  tanimoto_search(db, target, 0.5);
  close_database(db);
}
