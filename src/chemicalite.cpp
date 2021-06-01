#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include "settings.hpp"
#include "mol_formats.hpp"
#include "mol_chemtransforms.hpp"
#include "mol_compare.hpp"
#include "mol_descriptors.hpp"
#include "mol_hash.hpp"
#include "mol_props.hpp"
#include "mol_to_bfp.hpp"
#include "bfp_compare.hpp"
#include "bfp_descriptors.hpp"
#include "periodic_table.hpp"
#include "rdtree.hpp"
#include "sdf_io.hpp"
#include "versions.hpp"

/*
** Register the chemicalite module with database handle db.
*/
#ifdef _WIN32
__declspec(dllexport)
#endif
extern "C" int sqlite3_chemicalite_init(sqlite3 *db, char ** /*pzErrMsg*/,
			   const sqlite3_api_routines *pApi)
{
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi)

  if (rc == SQLITE_OK) rc = chemicalite_init_versions(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_settings(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_mol_formats(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_mol_chemtransforms(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_mol_compare(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_mol_descriptors(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_mol_hash(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_mol_props(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_mol_to_bfp(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_bfp_compare(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_bfp_descriptors(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_periodic_table(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_sdf_io(db);
  if (rc == SQLITE_OK) rc = chemicalite_init_rdtree(db);

  return rc;
}
