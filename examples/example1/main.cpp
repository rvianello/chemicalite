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


void initialize_database(sqlite3 * db)
{
  char * errmsg = 0;
  if (sqlite3_exec(db,
 		   "PRAGMA page_size=4096; "
		   "CREATE TABLE chembl("
		   "id INTEGER PRIMARY KEY, chembl_id TEXT, smiles TEXT, "
		   "molecule MOL); "
		   "SELECT create_molecule_rdtree('chembl', 'molecule')",  
		   NULL,  /* Callback function */
		   0,     /* 1st argument to callback */
		   &errmsg) != SQLITE_OK) {
    std::cerr << "An error occurred while initializing the db: "
	      << errmsg << std::endl;
    sqlite3_free(errmsg);
  }
}


void insert_molecules(sqlite3 * db, std::istream & input)
{
  const char * sql =
    "INSERT INTO chembl(chembl_id, smiles, molecule) VALUES(?, ?, mol(?))";

  sqlite3_stmt * stmt = 0;
  if (sqlite3_prepare_v2(db, sql, -1, & stmt, 0) != SQLITE_OK) {
    std::cerr << "Could not prepare sql statement" << std::endl;
  }
  
  // skip the header line
  char buffer[1024];
  input.getline(buffer, 1024);

  if (!input) {
    std::cerr << "Unexpected end of input after header line" << std::endl;
  }

  int line_count = 0;
  
  while (input) {
    std::string chembl_id, smiles, inchi, inchi_key;
    input >> chembl_id >> smiles >> inchi >> inchi_key;
    if (input) {
      // std::cout << ++line_count << " "
      // 		<< chembl_id << " " << smiles << std::endl;

      // bind the query parameter
      if (sqlite3_bind_text(stmt, 1, chembl_id.c_str(), -1, SQLITE_STATIC)
	  != SQLITE_OK) {
	std::cerr << "Couldn't bind chembl_id parameter" << std::endl;
	break;
      }
      if (sqlite3_bind_text(stmt, 2, smiles.c_str(), -1, SQLITE_STATIC)
	  != SQLITE_OK) {
	std::cerr << "Couldn't bind smiles parameter" << std::endl;
	break;
      }
      if (sqlite3_bind_text(stmt, 3, smiles.c_str(), -1, SQLITE_STATIC)
	  != SQLITE_OK) {
   	std::cerr << "Couldn't bind mol() parameter" << std::endl;
	break;
      }

      // execute the query
      if (sqlite3_step(stmt) != SQLITE_DONE) {
   	std::cerr << "Couldn't execute sql statement" << std::endl;
	break;
      }

      // reset the query
      if (sqlite3_reset(stmt) != SQLITE_OK) {
	 std::cerr << "Couldn't successfully reset sql statement" << std::endl;
      }

      // clear the bindings
      if (sqlite3_clear_bindings(stmt) != SQLITE_OK) {
	std::cerr << "Couldn't successfully clear the bindings" << std::endl;
      }
    }
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
  const char * input_path = argv[1];
  const char * db_path = argv[2];

  std::ifstream input_file(input_path);

  if (!input_file) {
    std::cerr << "Couldn't open input file: " << input_path << std::endl;
    return 1;
  }
  
  sqlite3 * db = open_database(db_path);

  load_chemicalite(db);
  initialize_database(db);
  insert_molecules(db, input_file);
  close_database(db);
}
