#include <assert.h>
#include <string.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "chemicalite.h"
#include "rdkit_adapter.h"
#include "utils.h"
#include "molecule.h"
#include "bitstring.h"
#include "logging.h"

/*
** fetch Bfp from blob argument
*/
static void free_bfp_auxdata(void * aux)
{
  free_bfp((Bfp *) aux);
}

int fetch_bfp_arg(sqlite3_value* arg, Bfp **ppBfp)
{
  int rc = SQLITE_OK;
  int value_type = sqlite3_value_type(arg);
  /* Check that value is a blob */
  if (value_type == SQLITE_BLOB) {
    int sz = sqlite3_value_bytes(arg);
    rc = blob_to_bfp(sqlite3_value_blob(arg), sz, ppBfp);
  }
  /* Or if it's a NULL - we will most often return NULL on NULL input*/
  else if (value_type == SQLITE_NULL) {
    *ppBfp = 0;
  }
  /* and if we don't know what to do with this value type */
  else {
    rc = SQLITE_MISMATCH;
    chemicalite_log(rc, "bfp args must be of type text, blob or NULL");
  }
  return rc;
}

/*
** Mol -> Bfp conversion
*/
#define MOL_TO_BFP(func) \
  static void func##_f(sqlite3_context* ctx, int argc, sqlite3_value** argv) \
  { \
    UNUSED(argc); \
    assert(argc == 1); \
    int rc = SQLITE_OK; \
  \
    Bfp * pBfp = 0; \
    u8 * pBlob = 0; \
    int sz = 0; \
  \
    void * aux = sqlite3_get_auxdata(ctx, 0); \
    if (aux) { \
      pBfp = (Bfp *) aux; \
    } \
    else { \
      Mol *pMol = 0; \
      rc = fetch_mol_arg(argv[0], &pMol); \
  \
      if (rc == SQLITE_OK && pMol) { \
        rc = func(pMol, &pBfp); \
      } \
  \
      if (rc == SQLITE_OK) { \
        sqlite3_set_auxdata(ctx, 0, (void *) pBfp, free_bfp_auxdata);	\
      } \
  \
      free_mol(pMol);	/* no-op if failed and pMol == 0 */ \
    } \
  \
    if (rc != SQLITE_OK) { \
      sqlite3_result_error_code(ctx, rc); \
      return; \
    } \
  \
    if (!pBfp) { \
      sqlite3_result_null(ctx); \
      return; \
    } \
  \
    rc = bfp_to_blob(pBfp, &pBlob, &sz); \
  \
    if (rc == SQLITE_OK) { \
      sqlite3_result_blob(ctx, pBlob, sz, sqlite3_free); \
    } \
    else { \
      sqlite3_result_error_code(ctx, rc); \
    } \
  }
    
MOL_TO_BFP(mol_layered_bfp)
MOL_TO_BFP(mol_rdkit_bfp)
MOL_TO_BFP(mol_atom_pairs_bfp)
MOL_TO_BFP(mol_topological_torsion_bfp)
MOL_TO_BFP(mol_maccs_bfp)

#define MOL_TO_MORGAN_BFP(func) \
  static void func##_f(sqlite3_context* ctx, \
		       int argc, sqlite3_value** argv) \
  { \
    UNUSED(argc); \
    assert(argc == 2); \
    int rc = SQLITE_OK; \
  \
    Bfp * pBfp = 0; \
    u8 * pBlob = 0; \
    int sz = 0; \
  \
    int radius = sqlite3_value_int(argv[1]); \
  \
    void * aux = sqlite3_get_auxdata(ctx, 0); \
    if (aux) { \
      pBfp = (Bfp *) aux; \
    } \
    else { \
      Mol *pMol = 0; \
      rc = fetch_mol_arg(argv[0], &pMol); \
  \
      if (rc == SQLITE_OK && pMol) { \
        rc = func(pMol, radius, &pBfp); \
      } \
  \
      if (rc == SQLITE_OK) { \
        sqlite3_set_auxdata(ctx, 0, (void *) pBfp, free_bfp_auxdata);	\
      } \
  \
      free_mol(pMol);	/* no-op if failed and pMol == 0 */ \
    } \
  \
    if (rc != SQLITE_OK) { \
      sqlite3_result_error_code(ctx, rc); \
      return; \
    } \
  \
    if (!pBfp) { \
      sqlite3_result_null(ctx); \
      return; \
    } \
  \
    rc = bfp_to_blob(pBfp, &pBlob, &sz); \
  \
    if (rc == SQLITE_OK) { \
      sqlite3_result_blob(ctx, pBlob, sz, sqlite3_free); \
    } \
    else { \
      sqlite3_result_error_code(ctx, rc); \
    } \
  }

MOL_TO_MORGAN_BFP(mol_morgan_bfp)
MOL_TO_MORGAN_BFP(mol_feat_morgan_bfp)

/*
** Slightly different, but still a bfp
*/
MOL_TO_BFP(mol_bfp_signature)

/*
** bitstring similarity
*/

#define COMPARE_BITSTRINGS(sim) \
  static void bfp_##sim##_f(sqlite3_context* ctx, \
			     int argc, sqlite3_value** argv) \
  { \
    UNUSED(argc); \
    assert(argc == 2); \
    double similarity = 0.; \
    int rc = SQLITE_OK; \
  \
    Bfp *p1 = 0; \
    Bfp *p2 = 0; \
	\
    void * aux1 = sqlite3_get_auxdata(ctx, 0); \
    if (aux1) { \
      p1 = (Bfp *) aux1; \
    } \
    else { \
      if ((rc = fetch_bfp_arg(argv[0], &p1)) != SQLITE_OK) { \
        sqlite3_result_error_code(ctx, rc); \
        return; \
      } \
      else { \
        sqlite3_set_auxdata(ctx, 0, (void *) p1, free_bfp_auxdata);	\
      } \
    } \
  \
    void * aux2 = sqlite3_get_auxdata(ctx, 1); \
    if (aux2) { \
      p2 = (Bfp *) aux2; \
    } \
    else { \
      if ((rc = fetch_bfp_arg(argv[1], &p2)) != SQLITE_OK) { \
        sqlite3_result_error_code(ctx, rc); \
        return; \
      } \
      else { \
        sqlite3_set_auxdata(ctx, 1, (void *) p2, free_bfp_auxdata);	\
      } \
    } \
	\
    if (!p1 || !p2) { \
      sqlite3_result_null(ctx); \
    } \
    else if (bfp_length(p1) != bfp_length(p2)) { \
      sqlite3_result_error_code(ctx, SQLITE_MISMATCH); \
    } \
    else { \
      similarity = bfp_##sim(p1, p2); \
      sqlite3_result_double(ctx, similarity); \
    } \
  }

/*
** bitstring similarity
*/
COMPARE_BITSTRINGS(tanimoto)
COMPARE_BITSTRINGS(dice)

/*
** build a simple bitstring (mostly for testing)
*/
static void bfp_dummy_f(sqlite3_context* ctx,
			int argc, sqlite3_value** argv)
{
  UNUSED(argc);
  assert(argc == 2);
  int rc = SQLITE_OK;
  int len, value;

  u8 * pBlob = 0;

  /* Check that value is a blob */
  if (sqlite3_value_type(argv[0]) != SQLITE_INTEGER) {
    rc = SQLITE_MISMATCH;
  }
  else if (sqlite3_value_type(argv[1]) != SQLITE_INTEGER) {
    rc = SQLITE_MISMATCH;
  }
  else {

    len = sqlite3_value_int(argv[0]);
    if (len <= 0) { len = 1; }
    if (len > MAX_BITSTRING_SIZE) { len = MAX_BITSTRING_SIZE; }

    value = sqlite3_value_int(argv[1]);
    if (value < 0) { value = 0; }
    if (value > 255) { value = 255; }

    pBlob = (u8 *)sqlite3_malloc(len);
    if (!pBlob) {
      rc = SQLITE_NOMEM;
    }
    else {
      u8 *p = pBlob; 
      int ii;
      for (ii = 0; ii < len; ++ii) {
        *p++ = value;
      }
    }

  }

  if (rc == SQLITE_OK) {
    sqlite3_result_blob(ctx, pBlob, len, sqlite3_free);
  }	
  else {
    sqlite3_result_error_code(ctx, rc);
  }

}

/*
** bitstring length and weight
*/

static void bfp_length_f(sqlite3_context* ctx,
			 int argc, sqlite3_value** argv)
{
  UNUSED(argc);
  assert(argc == 1);
  int rc = SQLITE_OK;
  
  Bfp *pBfp = 0;
  rc = fetch_bfp_arg(argv[0], &pBfp);

  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else if (!pBfp) {
    sqlite3_result_null(ctx);
  }
  else {
    int length = bfp_length(pBfp);
    free_bfp(pBfp);
    sqlite3_result_int(ctx, length);    
  }
}

static void bfp_weight_f(sqlite3_context* ctx,
			 int argc, sqlite3_value** argv)
{
  UNUSED(argc);
  assert(argc == 1);
  int rc = SQLITE_OK;
  
  Bfp *pBfp = 0;
  rc = fetch_bfp_arg(argv[0], &pBfp);

  if (rc != SQLITE_OK) {
    sqlite3_result_error_code(ctx, rc);
  }
  else if (!pBfp) {
    sqlite3_result_null(ctx);
  }
  else {
    int weight = bfp_weight(pBfp);
    free_bfp(pBfp);
    sqlite3_result_int(ctx, weight);    
  }
}

int chemicalite_init_bitstring(sqlite3 *db)
{
  int rc = SQLITE_OK;

  CREATE_SQLITE_BINARY_FUNCTION(bfp_tanimoto);
  CREATE_SQLITE_BINARY_FUNCTION(bfp_dice);

  CREATE_SQLITE_UNARY_FUNCTION(bfp_length);
  CREATE_SQLITE_UNARY_FUNCTION(bfp_weight);

  CREATE_SQLITE_UNARY_FUNCTION(mol_layered_bfp);
  CREATE_SQLITE_UNARY_FUNCTION(mol_rdkit_bfp);
  CREATE_SQLITE_UNARY_FUNCTION(mol_atom_pairs_bfp);
  CREATE_SQLITE_UNARY_FUNCTION(mol_topological_torsion_bfp);
  CREATE_SQLITE_UNARY_FUNCTION(mol_maccs_bfp);

  CREATE_SQLITE_BINARY_FUNCTION(mol_morgan_bfp);
  CREATE_SQLITE_BINARY_FUNCTION(mol_feat_morgan_bfp);

  CREATE_SQLITE_UNARY_FUNCTION(mol_bfp_signature);

  CREATE_SQLITE_BINARY_FUNCTION(bfp_dummy);

  return rc;
}
