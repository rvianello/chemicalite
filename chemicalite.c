#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "inttypes.h"
#include "utils.h"

#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include "object.c"
#include "molecule.c"

/*
** Register the chemicalite module with database handle db.
*/
int sqlite3_chemicalite_init(sqlite3 *db)
{
  int rc = SQLITE_OK;
  
  CREATE_SQLITE_UNARY_FUNCTION(mol, rc);
  CREATE_SQLITE_UNARY_FUNCTION(qmol, rc);
  
  CREATE_SQLITE_UNARY_FUNCTION(mol_smiles, rc);
  
  CREATE_SQLITE_BINARY_FUNCTION(mol_is_substruct, rc);
  CREATE_SQLITE_BINARY_FUNCTION(mol_is_superstruct, rc);
  CREATE_SQLITE_BINARY_FUNCTION(mol_cmp, rc);
  
  CREATE_SQLITE_UNARY_FUNCTION(mol_mw, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_logp, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_tpsa, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi0v, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi1v, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi2v, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi3v, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi4v, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi0n, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi1n, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi2n, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi3n, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi4n, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_kappa1, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_kappa2, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_kappa3, rc);

  CREATE_SQLITE_UNARY_FUNCTION(mol_hba, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_hbd, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_num_atms, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_num_hvyatms, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_num_rotatable_bnds, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_num_hetatms, rc);
  CREATE_SQLITE_UNARY_FUNCTION(mol_num_rings, rc);
  
  return rc;
}

int sqlite3_extension_init(sqlite3 *db, char **pzErrMsg,
			   const sqlite3_api_routines *pApi)
{
  SQLITE_EXTENSION_INIT2(pApi)
  return sqlite3_chemicalite_init(db);
}
