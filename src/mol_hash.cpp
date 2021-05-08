#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/RWMol.h>
#include <GraphMol/MolHash/MolHash.h>

#include "utils.hpp"
#include "mol_hash.hpp"
#include "mol.hpp"

template <RDKit::MolHash::HashFunction H>
static void mol_hash(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  sqlite3_value *arg = argv[0];

  int rc = SQLITE_OK;
  std::unique_ptr<RDKit::RWMol> mol(arg_to_rwmol(arg, &rc));

  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    std::string hash = RDKit::MolHash::MolHash(mol.get(), H);
    sqlite3_result_text(ctx, hash.c_str(), -1, SQLITE_TRANSIENT);
  }
}

int chemicalite_init_mol_hash(sqlite3 *db)
{
  int rc = SQLITE_OK;
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_anonymousgraph", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::AnonymousGraph>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_elementgraph", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::ElementGraph>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_canonicalsmiles", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::CanonicalSmiles>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_murckoscaffold", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::MurckoScaffold>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_extendedmurcko", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::ExtendedMurcko>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_molformula", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::MolFormula>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_atombondcounts", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::AtomBondCounts>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_degreevector", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::DegreeVector>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_mesomer", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::Mesomer>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_hetatomtautomer", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::HetAtomTautomer>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_hetatomprotomer", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::HetAtomProtomer>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_redoxpair", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::RedoxPair>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_regioisomer", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::Regioisomer>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_netcharge", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::NetCharge>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_smallworldindexbr", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::SmallWorldIndexBR>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_smallworldindexbrl", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::SmallWorldIndexBRL>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "mol_hash_arthorsubstructureorder", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<mol_hash<RDKit::MolHash::HashFunction::ArthorSubstructureOrder>>, 0, 0);
  return rc;
}
