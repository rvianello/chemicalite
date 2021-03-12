#include <utility>

#include "test_common.hpp"

TEST_CASE("mol hash", "[mol]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  SECTION("test mol hash")
  {
    for (const auto & test_data: {
      std::pair<const char *, const char *>{"mol_hash_anonymousgraph", "***1****(*2*****2*)*1"},
      {"mol_hash_elementgraph", "COC1CC(C2CCCCC2O)CCN1"},
      {"mol_hash_canonicalsmiles", "COc1cc(C2CCCCC2O)ccn1"},
      {"mol_hash_murckoscaffold", "c1cc(C2CCCCC2)ccn1"},
      {"mol_hash_extendedmurcko", "*c1cc(C2CCCCC2*)ccn1"},
      {"mol_hash_molformula", "C12H17NO2"},
      {"mol_hash_atombondcounts", "15,16"},
      {"mol_hash_degreevector", "0,4,9,2"},
      {"mol_hash_mesomer", "CO[C]1[CH][C](C2CCCCC2O)[CH][CH][N]1_0"},
      {"mol_hash_regioisomer", "*O.*O*.C.C1CCCCC1.c1ccncc1"},
      {"mol_hash_netcharge", "0"},
      {"mol_hash_smallworldindexbr", "B16R2"},
      {"mol_hash_smallworldindexbrl", "B16R2L9"},
      {"mol_hash_arthorsubstructureorder", "000f001001000c000300005f000000"}
    }) {
      std::string mol_hash = test_data.first;
      std::string expected = test_data.second;  
      std::string query = "SELECT " + mol_hash + "(mol_from_smiles('C1CCCC(O)C1c1ccnc(OC)c1'))";

      test_select_value(db, query, expected);
    }
  }

  test_db_close(db);
}
