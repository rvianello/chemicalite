#include <memory>

#include <sqlite3.h>
#include <catch2/catch.hpp>

#include <GraphMol/SmilesParse/SmilesParse.h>
#include <GraphMol/FileParsers/FileParsers.h>
#include <GraphMol/SmilesParse/SmilesWrite.h>

static const char * smiles_data[] = { 
  "c1ccccc1",
  "CCC1CCNCC1" 
};

TEST_CASE("mol formats interconversion", "[mol]")
{
  sqlite3 * db = nullptr;
  int rc = SQLITE_OK;

  // Create a connection to an in-memory database
  rc = sqlite3_open(":memory:", &db);
  REQUIRE(rc == SQLITE_OK);

  // Enable loading extensions
  rc = sqlite3_enable_load_extension(db, 1);
  REQUIRE(rc == SQLITE_OK);

  // Load ChemicaLite
  rc = sqlite3_load_extension(db, "chemicalite", 0, 0);
  REQUIRE(rc == SQLITE_OK);

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

      std::string molblock_output = (const char *) sqlite3_column_text(pStmt, 0);

      std::unique_ptr<RDKit::ROMol> input_mol(RDKit::SmilesToMol(smiles_input));
      std::unique_ptr<RDKit::ROMol> output_mol(RDKit::MolBlockToMol(molblock_output));
      REQUIRE(RDKit::MolToSmiles(*input_mol) == RDKit::MolToSmiles(*output_mol));

      rc = sqlite3_reset(pStmt);
      REQUIRE(rc == SQLITE_OK);
    }

    sqlite3_finalize(pStmt);
  }

  // Close the db
  rc = sqlite3_close(db);
  REQUIRE(rc == SQLITE_OK);
}