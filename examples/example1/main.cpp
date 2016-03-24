#include <sqlite3.h>

sqlite3 * create_database(const char * filename)
{
  sqlite3 * db;
  if (sqlite3_open(filename, &db) != SQLITE_OK) {
  }
  return db;
}

int main(int argc, char * argv[])
{
  const char * db_path = argv[1];
  //const char * input_path = argv[2];
  
  sqlite3 * db = create_database(db_path);
  
}
