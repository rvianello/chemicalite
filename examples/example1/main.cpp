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
		   "CREATE TABLE compounds("
		   "id INTEGER PRIMARY KEY, label TEXT, smiles TEXT, "
		   "molecule MOL);"
		   "PRAGMA journal_mode=MEMORY",  
		   NULL,  /* Callback function */
		   0,     /* 1st argument to callback */
		   &errmsg) != SQLITE_OK) {
    std::cerr << "An error occurred while initializing the db: "
	      << errmsg << std::endl;
    sqlite3_free(errmsg);
  }
}


void replace_substring(std::string & str,
		       const std::string & from, const std::string & to)
{
  if(from.empty())
    return;
  size_t start_pos = 0;
  while((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
  }
}


void fix_smiles(std::string & smiles)
{
  replace_substring(smiles, "=N#N", "=[N+]=[N-]");
  replace_substring(smiles, "N#N=", "[N-]=[N+]=");
}


void insert_molecules(sqlite3 * db, std::istream & input)
{
  char * errmsg = 0;

  if (sqlite3_exec(db, "BEGIN",  
		   NULL,  /* Callback function */
		   0,     /* 1st argument to callback */
		   &errmsg) != SQLITE_OK) {
    std::cerr << "Could not begin transaction: " << errmsg << std::endl;
    sqlite3_free(errmsg);
  }

  const char * sql =
    "INSERT INTO compounds(label, smiles, molecule) VALUES(?, ?, mol(?))";

  sqlite3_stmt * stmt = 0;
  if (sqlite3_prepare_v2(db, sql, -1, & stmt, 0) != SQLITE_OK) {
    std::cerr << "Could not prepare sql statement" << std::endl;
  }
  
  // skip the header line
  static const int BUFFER_SIZE = 4096;
  char buffer[BUFFER_SIZE];
  input.getline(buffer, BUFFER_SIZE);

  if (!input) {
    std::cerr << "Unexpected end of input after header line" << std::endl;
  }

  int line_count = 0;
  
  while (input) {

    std::string label, smiles;
    input >> label >> smiles;
    input.getline(buffer, BUFFER_SIZE); // discard the rest of the line
    
    if (input) {

      ++line_count;

      if (smiles.size() > 300) {
	continue;
      }

      fix_smiles(smiles);
      
      // bind the query parameter
      if (sqlite3_bind_text(stmt, 1, label.c_str(), -1, SQLITE_STATIC)
	  != SQLITE_OK) {
	std::cerr << "Couldn't bind label parameter" << std::endl;
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
   	std::cerr << "Couldn't execute sql statement: "
		  << line_count << std::endl;
	// don't break; just reset the statement and continue.
      }

      // reset the query
      if (sqlite3_reset(stmt) != SQLITE_OK) {
	// an error code is returned if the statement failed executing
	// so the output below may appear misleading
	//std::cerr << "Couldn't successfully reset sql statement" << std::endl;
      }

      // clear the bindings
      if (sqlite3_clear_bindings(stmt) != SQLITE_OK) {
	std::cerr << "Couldn't successfully clear the bindings" << std::endl;
      }
    }
  }

  if (sqlite3_finalize(stmt) != SQLITE_OK)  {
    //std::cerr << "Couldn't finalize statement" << std::endl;
  }
  
  if (sqlite3_exec(db, "COMMIT",  
		   NULL,  /* Callback function */
		   0,     /* 1st argument to callback */
		   &errmsg) != SQLITE_OK) {
    std::cerr << "Could not commit transaction: " << errmsg << std::endl;
    sqlite3_free(errmsg);
  }
}


void create_index(sqlite3 * db)
{
  char * errmsg = 0;
  if (sqlite3_exec(db,
 		   "SELECT create_molecule_rdtree('compounds', 'molecule')",  
		   NULL,  /* Callback function */
		   0,     /* 1st argument to callback */
		   &errmsg) != SQLITE_OK) {
    std::cerr << "An error occurred while indexing the mol column: "
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
  create_index(db);

  close_database(db);
}
