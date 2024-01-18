#include "test_common.hpp"


TEST_CASE("periodic table", "[pte]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  SECTION("select by atomic number")
  {
    test_select_value(db, "SELECT symbol FROM periodic_table WHERE atomic_number = 6", "C");
    test_select_value(db, "SELECT atomic_weight FROM periodic_table WHERE atomic_number = 7", 14.007);
    test_select_value(db, "SELECT vdw_radius FROM periodic_table WHERE atomic_number = 8", 1.55);
    test_select_value(db, "SELECT covalent_radius FROM periodic_table WHERE atomic_number = 9", 0.57);
  }

  SECTION("select by symbol")
  {
    test_select_value(db, "SELECT atomic_number FROM periodic_table WHERE symbol = 'C'", 6);
    test_select_value(db, "SELECT atomic_weight FROM periodic_table WHERE symbol = 'N'", 14.007);
    test_select_value(db, "SELECT vdw_radius FROM periodic_table WHERE symbol = 'O'", 1.55);
    test_select_value(db, "SELECT covalent_radius FROM periodic_table WHERE symbol = 'F'", 0.57);
  }

  test_db_close(db);
}
