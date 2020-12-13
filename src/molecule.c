#include <assert.h>
#include <string.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "chemicalite.h"
#include "rdkit_adapter.h"
#include "utils.h"
#include "molecule.h"
#include "logging.h"

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
  int rc = SQLITE_OK;
  int value_type = sqlite3_value_type(arg);
  /* Check that value is a blob */
  if (value_type == SQLITE_BLOB) {
    int sz = sqlite3_value_bytes(arg);
    rc = blob_to_mol(sqlite3_value_blob(arg), sz, ppMol);
  }
  /* or a text string - by default assumed to be a SMILES */
  else if (value_type == SQLITE3_TEXT) {
    if (sqlite3_value_bytes(arg) > MOL_MAX_TXT_LENGTH) {
      rc = SQLITE_TOOBIG;
      chemicalite_log(
        rc,
        "Input string exceeds the maximum allowed length for a mol text representation (%d)",
        MOL_MAX_TXT_LENGTH
        );
    } else {
      /* txt_to_mol will return an error if the conversion failed
      ** resulting in a null pointer. the error is not captured here
      ** because the null Mol pointer will result in a NULL SQL result
      */
      txt_to_mol((const char *)sqlite3_value_text(arg), AS_SMILES, ppMol);
    }
  }
  /* if NULL, return a null pointer - will most often want to return a NULL ON NULL */
  else if (value_type == SQLITE_NULL) {
    *ppMol = NULL;
  }
  /* finally if it's not a value type we can use, return an error */
  else {
    rc = SQLITE_MISMATCH;
    chemicalite_log(rc, "mol args must be of type text, blob or NULL");
  }
  return rc;
}

/*
** implementation for SMILES/SMARTS conversion to molecule object 
*/
static void cast_to_molecule(sqlite3_context* ctx, 
			     int argc, sqlite3_value** argv, int mode)
{
  UNUSED(argc);
  assert(argc == 1);
  sqlite3_value *arg = argv[0];

  int value_type = sqlite3_value_type(arg);

  /* build the molecule binary repr from a text string */
  if (value_type == SQLITE3_TEXT) {
    
    if (sqlite3_value_bytes(arg) > MOL_MAX_TXT_LENGTH) {
      chemicalite_log(
        SQLITE_TOOBIG,
        "Input string exceeds the maximum allowed length for a %s text representation (%d)",
        mode == AS_SMARTS ? "qmol" : "mol",
        MOL_MAX_TXT_LENGTH
        );
      sqlite3_result_error_toobig(ctx);
      return;
    }

    int rc;

    Mol *pMol = 0;
    rc = txt_to_mol((const char *)sqlite3_value_text(arg), mode, &pMol);
    if (rc != SQLITE_OK) {
      sqlite3_result_null(ctx);
      return;
    }

    u8 *pBlob = 0;
    int sz = 0;
    rc = mol_to_blob(pMol, &pBlob, &sz);
    if (rc != SQLITE_OK) {
      sqlite3_result_error_code(ctx, rc);
      return;
    }

    sqlite3_result_blob(ctx, pBlob, sz, sqlite3_free);

  }
  /* not a string */
  else if (value_type == SQLITE_NULL) {
    sqlite3_result_null(ctx);
  }
  else {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    chemicalite_log(
      SQLITE_MISMATCH,
      "%s args must be of type text, blob or NULL",
      mode == AS_SMARTS ? "qmol" : "mol"
      );
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
  UNUSED(argc);
  assert(argc == 1);
  int rc = SQLITE_OK;

  Mol *pMol = 0;
  char * smiles = 0;

  if ( (rc = fetch_mol_arg(argv[0], &pMol)) != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
  }
  else if (pMol == 0) {
    sqlite3_result_null(ctx);
  }
  else if ( (rc = mol_to_txt(pMol, AS_SMILES, &smiles)) != SQLITE_OK ) {
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

#define COMPARE_STRUCTURES(func) \
  static void func##_f(sqlite3_context* ctx, int argc, sqlite3_value** argv) \
  { \
    UNUSED(argc); \
    assert(argc == 2); \
    int rc = SQLITE_OK; \
  \
    Mol *p1 = 0; \
    Mol *p2 = 0; \
  \
    void * aux1 = sqlite3_get_auxdata(ctx, 0); \
    if (aux1) { \
      p1 = (Mol *) aux1; \
    } \
    else { \
      if ((rc = fetch_mol_arg(argv[0], &p1)) != SQLITE_OK) { \
        sqlite3_result_error_code(ctx, rc); \
        return; \
      } \
      else { \
        sqlite3_set_auxdata(ctx, 0, (void *) p1, free_mol_auxdata); \
      } \
    } \
	\
    void * aux2 = sqlite3_get_auxdata(ctx, 1); \
    if (aux2) { \
      p2 = (Mol *) aux2; \
    }	\
    else { \
      if ((rc = fetch_mol_arg(argv[1], &p2)) != SQLITE_OK) { \
        sqlite3_result_error_code(ctx, rc); \
        return; \
      } \
      else { \
        sqlite3_set_auxdata(ctx, 1, (void *) p2, free_mol_auxdata);	\
      } \
    } \
	\
    if (p1 && p2) { \
      int result = func(p1, p2); \
      sqlite3_result_int(ctx, result); \
    } \
    else { \
      sqlite3_result_null(ctx); \
    } \
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
  { \
    UNUSED(argc); \
    assert(argc == 1); \
	\
    Mol *pMol = 0; \
    int rc = fetch_mol_arg(argv[0], &pMol); \
	\
    if (rc != SQLITE_OK) { \
      sqlite3_result_error_code(ctx, rc); \
    } \
    else if (!pMol) { \
      sqlite3_result_null(ctx); \
    } \
    else { \
      type descriptor = func(pMol); \
      free_mol(pMol); \
      sqlite3_result_##type(ctx, descriptor); \
    } \
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
  
  CREATE_SQLITE_UNARY_FUNCTION(mol);
  CREATE_SQLITE_UNARY_FUNCTION(qmol);
  
  CREATE_SQLITE_UNARY_FUNCTION(mol_smiles);
  
  CREATE_SQLITE_BINARY_FUNCTION(mol_is_substruct);
  CREATE_SQLITE_BINARY_FUNCTION(mol_is_superstruct);
  CREATE_SQLITE_BINARY_FUNCTION(mol_cmp);
  
  CREATE_SQLITE_UNARY_FUNCTION(mol_mw);
  CREATE_SQLITE_UNARY_FUNCTION(mol_logp);
  CREATE_SQLITE_UNARY_FUNCTION(mol_tpsa);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi0v);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi1v);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi2v);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi3v);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi4v);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi0n);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi1n);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi2n);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi3n);
  CREATE_SQLITE_UNARY_FUNCTION(mol_chi4n);
  CREATE_SQLITE_UNARY_FUNCTION(mol_kappa1);
  CREATE_SQLITE_UNARY_FUNCTION(mol_kappa2);
  CREATE_SQLITE_UNARY_FUNCTION(mol_kappa3);

  CREATE_SQLITE_UNARY_FUNCTION(mol_hba);
  CREATE_SQLITE_UNARY_FUNCTION(mol_hbd);
  CREATE_SQLITE_UNARY_FUNCTION(mol_num_atms);
  CREATE_SQLITE_UNARY_FUNCTION(mol_num_hvyatms);
  CREATE_SQLITE_UNARY_FUNCTION(mol_num_rotatable_bnds);
  CREATE_SQLITE_UNARY_FUNCTION(mol_num_hetatms);
  CREATE_SQLITE_UNARY_FUNCTION(mol_num_rings);
  
  return rc;
}
