#include <memory>

#include "test_common.hpp"

#include <GraphMol/SmilesParse/SmilesParse.h>
#include <GraphMol/FileParsers/FileParsers.h>
#include <GraphMol/SmilesParse/SmilesWrite.h>
#include <GraphMol/MolPickler.h>

static const char * smiles_data[] = { 
  "c1ccccc1",
  "CCC1CCNCC1" 
};

static const char * molblock_data[] = {
  R"(
     RDKit          2D

  6  6  0  0  0  0  0  0  0  0999 V2000
    1.5000    0.0000    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0
    0.7500   -1.2990    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0
   -0.7500   -1.2990    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0
   -1.5000    0.0000    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0
   -0.7500    1.2990    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0
    0.7500    1.2990    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0
  1  2  2  0
  2  3  1  0
  3  4  2  0
  4  5  1  0
  5  6  2  0
  6  1  1  0
M  END
)",
  R"(
     RDKit          2D

  8  8  0  0  0  0  0  0  0  0999 V2000
    3.7500   -1.2990    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0
    3.0000    0.0000    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0
    1.5000    0.0000    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0
    0.7500   -1.2990    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0
   -0.7500   -1.2990    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0
   -1.5000    0.0000    0.0000 N   0  0  0  0  0  0  0  0  0  0  0  0
   -0.7500    1.2990    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0
    0.7500    1.2990    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0
  1  2  1  0
  2  3  1  0
  3  4  1  0
  4  5  1  0
  5  6  1  0
  6  7  1  0
  7  8  1  0
  8  3  1  0
M  END
)"
};

TEST_CASE("mol formats interconversion", "[mol]")
{
  sqlite3 * db = nullptr;
  test_db_open(&db);

  int rc = SQLITE_OK;

  SECTION("smiles to smiles")
  {
    sqlite3_stmt *pStmt;
    rc = sqlite3_prepare_v2(db, "SELECT mol_to_smiles(mol_from_smiles(:smiles))", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    for (const char * smiles_input : smiles_data) {
      rc = sqlite3_bind_text(pStmt, 1, smiles_input, -1, SQLITE_STATIC);
      REQUIRE(rc == SQLITE_OK);

      rc = sqlite3_step(pStmt);
      REQUIRE(rc == SQLITE_ROW);

      int value_type = sqlite3_column_type(pStmt, 0);
      REQUIRE(value_type == SQLITE_TEXT);

      std::string smiles_output = (const char *) sqlite3_column_text(pStmt, 0);

      std::unique_ptr<RDKit::ROMol> input_mol(RDKit::SmilesToMol(smiles_input));
      std::unique_ptr<RDKit::ROMol> output_mol(RDKit::SmilesToMol(smiles_output));
      REQUIRE(RDKit::MolToSmiles(*input_mol) == RDKit::MolToSmiles(*output_mol));

      rc = sqlite3_reset(pStmt);
      REQUIRE(rc == SQLITE_OK);
    }

    sqlite3_finalize(pStmt);
  }

  SECTION("smiles to molblock")
  {
    sqlite3_stmt *pStmt;
    rc = sqlite3_prepare_v2(db, "SELECT mol_to_molblock(mol_from_smiles(:smiles))", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    for (const char * smiles_input : smiles_data) {
      rc = sqlite3_bind_text(pStmt, 1, smiles_input, -1, SQLITE_STATIC);
      REQUIRE(rc == SQLITE_OK);

      rc = sqlite3_step(pStmt);
      REQUIRE(rc == SQLITE_ROW);

      int value_type = sqlite3_column_type(pStmt, 0);
      REQUIRE(value_type == SQLITE_TEXT);

      std::string molblock_output = (const char *) sqlite3_column_text(pStmt, 0);

      std::unique_ptr<RDKit::ROMol> input_mol(RDKit::SmilesToMol(smiles_input));
      std::unique_ptr<RDKit::ROMol> output_mol(RDKit::MolBlockToMol(molblock_output));
      REQUIRE(RDKit::MolToSmiles(*input_mol) == RDKit::MolToSmiles(*output_mol));

      rc = sqlite3_reset(pStmt);
      REQUIRE(rc == SQLITE_OK);
    }

    sqlite3_finalize(pStmt);
  }

  SECTION("molblock to smiles")
  {
    sqlite3_stmt *pStmt;
    rc = sqlite3_prepare_v2(db, "SELECT mol_to_smiles(mol_from_molblock(:molblock))", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    for (const char * molblock_input : molblock_data) {
      rc = sqlite3_bind_text(pStmt, 1, molblock_input, -1, SQLITE_STATIC);
      REQUIRE(rc == SQLITE_OK);

      rc = sqlite3_step(pStmt);
      REQUIRE(rc == SQLITE_ROW);

      int value_type = sqlite3_column_type(pStmt, 0);
      REQUIRE(value_type == SQLITE_TEXT);

      std::string smiles_output = (const char *) sqlite3_column_text(pStmt, 0);

      std::unique_ptr<RDKit::ROMol> input_mol(RDKit::MolBlockToMol(molblock_input));
      std::unique_ptr<RDKit::ROMol> output_mol(RDKit::SmilesToMol(smiles_output));
      REQUIRE(RDKit::MolToSmiles(*input_mol) == RDKit::MolToSmiles(*output_mol));

      rc = sqlite3_reset(pStmt);
      REQUIRE(rc == SQLITE_OK);
    }

    sqlite3_finalize(pStmt);
  }

  SECTION("molblock to molblock")
  {
    sqlite3_stmt *pStmt;
    rc = sqlite3_prepare_v2(db, "SELECT mol_to_molblock(mol_from_molblock(:molblock))", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    for (const char * molblock_input : molblock_data) {
      rc = sqlite3_bind_text(pStmt, 1, molblock_input, -1, SQLITE_STATIC);
      REQUIRE(rc == SQLITE_OK);

      rc = sqlite3_step(pStmt);
      REQUIRE(rc == SQLITE_ROW);

      int value_type = sqlite3_column_type(pStmt, 0);
      REQUIRE(value_type == SQLITE_TEXT);

      std::string molblock_output = (const char *) sqlite3_column_text(pStmt, 0);

      std::unique_ptr<RDKit::ROMol> input_mol(RDKit::MolBlockToMol(molblock_input));
      std::unique_ptr<RDKit::ROMol> output_mol(RDKit::MolBlockToMol(molblock_output));
      REQUIRE(RDKit::MolToSmiles(*input_mol) == RDKit::MolToSmiles(*output_mol));

      rc = sqlite3_reset(pStmt);
      REQUIRE(rc == SQLITE_OK);
    }

    sqlite3_finalize(pStmt);
  }

  SECTION("smiles to binary mol")
  {
    sqlite3_stmt *pStmt;
    rc = sqlite3_prepare_v2(db, "SELECT mol_to_binary_mol(mol_from_smiles(:smiles))", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    for (const char * smiles_input : smiles_data) {
      rc = sqlite3_bind_text(pStmt, 1, smiles_input, -1, SQLITE_STATIC);
      REQUIRE(rc == SQLITE_OK);

      rc = sqlite3_step(pStmt);
      REQUIRE(rc == SQLITE_ROW);

      int value_type = sqlite3_column_type(pStmt, 0);
      REQUIRE(value_type == SQLITE_BLOB);

      int sz = sqlite3_column_bytes(pStmt, 0);
      std::string pkl((const char *)sqlite3_column_blob(pStmt, 0), sz);
      std::unique_ptr<RDKit::ROMol> input_mol(RDKit::SmilesToMol(smiles_input));
      std::unique_ptr<RDKit::ROMol> output_mol(new RDKit::ROMol(pkl));
      REQUIRE(RDKit::MolToSmiles(*input_mol) == RDKit::MolToSmiles(*output_mol));

      rc = sqlite3_reset(pStmt);
      REQUIRE(rc == SQLITE_OK);
    }

    sqlite3_finalize(pStmt);
  }

  SECTION("binary mol to smiles")
  {
    sqlite3_stmt *pStmt;
    rc = sqlite3_prepare_v2(db, "SELECT mol_to_smiles(mol_from_binary_mol(:bmol))", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    for (const char * smiles_input : smiles_data) {
      std::unique_ptr<RDKit::ROMol> input_mol(RDKit::SmilesToMol(smiles_input));
      std::string pkl;
      RDKit::MolPickler::pickleMol(
        *input_mol, pkl,
        RDKit::PicklerOps::AllProps | RDKit::PicklerOps::CoordsAsDouble);

      rc = sqlite3_bind_blob(pStmt, 1, pkl.c_str(), pkl.size(), SQLITE_TRANSIENT);
      REQUIRE(rc == SQLITE_OK);

      rc = sqlite3_step(pStmt);
      REQUIRE(rc == SQLITE_ROW);

      int value_type = sqlite3_column_type(pStmt, 0);
      REQUIRE(value_type == SQLITE_TEXT);

      std::string smiles_output = (const char *) sqlite3_column_text(pStmt, 0);
      std::unique_ptr<RDKit::ROMol> output_mol(RDKit::SmilesToMol(smiles_output));

      REQUIRE(RDKit::MolToSmiles(*input_mol) == RDKit::MolToSmiles(*output_mol));

      rc = sqlite3_reset(pStmt);
      REQUIRE(rc == SQLITE_OK);
    }

    sqlite3_finalize(pStmt);
  }

  SECTION("smarts roundtrip")
  {
    sqlite3_stmt *pStmt;
    rc = sqlite3_prepare_v2(db, "SELECT mol_to_smarts(mol_from_smarts(:smarts))", -1, &pStmt, 0);
    REQUIRE(rc == SQLITE_OK);

    for (const std::string & smarts: {"CC[C,N]C"}) {
      rc = sqlite3_bind_text(pStmt, 1, smarts.c_str(), smarts.size(), SQLITE_TRANSIENT);
      REQUIRE(rc == SQLITE_OK);

      rc = sqlite3_step(pStmt);
      REQUIRE(rc == SQLITE_ROW);

      int value_type = sqlite3_column_type(pStmt, 0);
      REQUIRE(value_type == SQLITE_TEXT);

      std::string smarts_output = (const char *) sqlite3_column_text(pStmt, 0);
      REQUIRE(smarts == smarts_output);

      rc = sqlite3_reset(pStmt);
      REQUIRE(rc == SQLITE_OK);
    }

    sqlite3_finalize(pStmt);
  }

  test_db_close(db);
}
