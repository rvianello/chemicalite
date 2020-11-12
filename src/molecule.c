#include <assert.h>
#include <string.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "chemicalite.h"
#include "rdkit_adapter.h"
#include "utils.h"
#include "molecule.h"

static const int MOL_MAX_TXT_LENGTH = 2048;
static const int AS_SMILES = 0;
static const int AS_SMARTS = 1;

/*
** fetch Mol from text or blob argument of molobj type
*/
static void free_mol_auxdata(void * aux)
{
  free_mol((Mol *) aux);
}

int fetch_mol_arg(sqlite3_value* arg, Mol **ppMol)
{
  int rc = SQLITE_MISMATCH;
  /* Check that value is a blob */
  if (sqlite3_value_type(arg) == SQLITE_BLOB) {
    int sz = sqlite3_value_bytes(arg);
    rc = blob_to_mol(sqlite3_value_blob(arg), sz, ppMol);
  }
  /* or a text string - by default assumed to be a SMILES */
  else if (sqlite3_value_type(arg) == SQLITE3_TEXT) {
    rc = sqlite3_value_bytes(arg) <= MOL_MAX_TXT_LENGTH ?
      txt_to_mol((const char *)sqlite3_value_text(arg), AS_SMILES, ppMol) : SQLITE_TOOBIG;
  }
  return rc;
}

/*
** implementation for SMILES/SMARTS conversion to molecule object 
*/
static void cast_to_molecule(sqlite3_context* ctx, 
			     int argc, sqlite3_value** argv, int mode)
{
  assert(argc == 1); argc = argc;
  sqlite3_value *arg = argv[0];

  /* build the molecule binary repr from a text string */
  if (sqlite3_value_type(arg) == SQLITE3_TEXT) {
    
    if (sqlite3_value_bytes(arg) > MOL_MAX_TXT_LENGTH) {
      sqlite3_result_error_toobig(ctx);
      return;
    }

    u8 *pBlob = 0;
    int sz = 0;
    int rc = txt_to_blob((const char *)sqlite3_value_text(arg), mode, &pBlob, &sz);
    if (rc != SQLITE_OK) {
      sqlite3_result_error_code(ctx, rc);
      return;
    }

    sqlite3_result_blob(ctx, pBlob, sz, sqlite3_free);

  }
  /* not a string */
  else {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
  }
}

/*
** convert to molecule
*/
static void mol_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  cast_to_molecule(ctx, argc, argv, AS_SMILES);
}

/*
** Convert to query molecules
*/
static void qmol_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  cast_to_molecule(ctx, argc, argv, AS_SMARTS);
}

/*
** convert molecule into a SMILES string
*/
static void mol_smiles_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  assert(argc == 1); argc = argc;
  int rc = SQLITE_OK;

  Mol *pMol = 0;
  char * smiles = 0;

  if ( ((rc = fetch_mol_arg(argv[0], &pMol)) != SQLITE_OK) ||
       ((rc = mol_to_txt(pMol, AS_SMILES, &smiles)) != SQLITE_OK) ) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    sqlite3_result_text(ctx, smiles, -1, sqlite3_free);
  }

  free_mol(pMol);
}

/*
** structural comparison
*/

#define COMPARE_STRUCTURES(func)					\
  static void func##_f(sqlite3_context* ctx, int argc, sqlite3_value** argv) \
  {									\
    assert(argc == 2); argc = argc; \
    int rc = SQLITE_OK;							\
									\
    Mol *p1 = 0;							\
    Mol *p2 = 0;							\
									\
    void * aux1 = sqlite3_get_auxdata(ctx, 0);				\
    if (aux1) {								\
      p1 = (Mol *) aux1;						\
    }									\
    else {								\
      if ((rc = fetch_mol_arg(argv[0], &p1)) != SQLITE_OK) {		\
	goto func##_f_end;						\
      }									\
      else {								\
	sqlite3_set_auxdata(ctx, 0, (void *) p1, free_mol_auxdata);	\
      }									\
    }									\
									\
    void * aux2 = sqlite3_get_auxdata(ctx, 1);				\
    if (aux2) {								\
      p2 = (Mol *) aux2;						\
    }									\
    else {								\
      if ((rc = fetch_mol_arg(argv[1], &p2)) != SQLITE_OK) {		\
	goto func##_f_end;						\
      }									\
      else {								\
	sqlite3_set_auxdata(ctx, 1, (void *) p2, free_mol_auxdata);	\
      }									\
    }									\
									\
    int result = func(p1, p2);						\
									\
  func##_f_end:								\
    if (rc == SQLITE_OK) {						\
      sqlite3_result_int(ctx, result);					\
    }									\
    else {								\
      sqlite3_result_error_code(ctx, rc);				\
    }									\
  }

/*
** substructure match and structural comparison (ordering)
*/
COMPARE_STRUCTURES(mol_is_substruct)
COMPARE_STRUCTURES(mol_is_superstruct)
COMPARE_STRUCTURES(mol_cmp)

/*
** molecular descriptors
*/

#define MOL_DESCRIPTOR(func, type) \
  static void func##_f(sqlite3_context* ctx, int argc, sqlite3_value** argv) \
  {									\
    assert(argc == 1); argc = argc; \
									\
    Mol *pMol = 0;							\
    int rc = fetch_mol_arg(argv[0], &pMol);				\
									\
    if (rc == SQLITE_OK) {						\
      type descriptor = func(pMol);					\
      free_mol(pMol);							\
      sqlite3_result_##type(ctx, descriptor);				\
    }									\
    else {								\
      sqlite3_result_error_code(ctx, rc);				\
    }									\
  }

MOL_DESCRIPTOR(mol_mw, double)
MOL_DESCRIPTOR(mol_logp, double)
MOL_DESCRIPTOR(mol_tpsa, double)
MOL_DESCRIPTOR(mol_chi0v, double)
MOL_DESCRIPTOR(mol_chi1v, double)
MOL_DESCRIPTOR(mol_chi2v, double)
MOL_DESCRIPTOR(mol_chi3v, double)
MOL_DESCRIPTOR(mol_chi4v, double)
MOL_DESCRIPTOR(mol_chi0n, double)
MOL_DESCRIPTOR(mol_chi1n, double)
MOL_DESCRIPTOR(mol_chi2n, double)
MOL_DESCRIPTOR(mol_chi3n, double)
MOL_DESCRIPTOR(mol_chi4n, double)
MOL_DESCRIPTOR(mol_kappa1, double)
MOL_DESCRIPTOR(mol_kappa2, double)
MOL_DESCRIPTOR(mol_kappa3, double)

MOL_DESCRIPTOR(mol_hba, int)
MOL_DESCRIPTOR(mol_hbd, int)
MOL_DESCRIPTOR(mol_num_atms, int)
MOL_DESCRIPTOR(mol_num_hvyatms, int)
MOL_DESCRIPTOR(mol_num_rotatable_bnds, int)
MOL_DESCRIPTOR(mol_num_hetatms, int)
MOL_DESCRIPTOR(mol_num_rings, int)

int chemicalite_init_molecule(sqlite3 *db)
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
