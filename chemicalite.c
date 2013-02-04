#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "inttypes.h"

#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include "rdkit_adapter.h"

/*
** data type structure (blob wrappers for chemicalite objects)
*/

typedef struct Object Object;

struct Object {
  u32 marker;
  u8 blob[];
};
 
static const u32 MAGIC_MASK = 0xFFFFFF00;
static const u32 TYPE_MASK = 0x000000FF;
static const u32 MAGIC = 0xABCDEF00;

static const u32 MOLOBJ = 0x00000001;
static const u32 QMOLOBJ = 0x00000002;

#define OBJMAGIC(p) (*((u32 *)p) & MAGIC_MASK)
#define OBJTYPE(p) (*((u32 *)p) & TYPE_MASK)
#define IS_OBJPTR(p) (OBJMAGIC(p) == MAGIC)
#define IS_MOLOBJ(p) (IS_OBJPTR(p) && (OBJTYPE(p) == MOLOBJ))
#define IS_QMOLOBJ(p) (IS_OBJPTR(p) && (OBJTYPE(p) == QMOLOBJ))

/* 
** wraps the binary blob pointed by pBlob in an Object structure where
** it's prefixed by a data type marker
*/
static int wrap_object(u8 *pBlob, int sz, u32 type, 
		       Object **ppObject, int *pObjSz)
{
  int rc = SQLITE_NOMEM;
  int objsz = sizeof(Object) + sz;
  *ppObject = sqlite3_malloc(objsz);
  if (*ppObject) {
    *pObjSz = objsz;
    (*ppObject)->marker = MAGIC | type;
    memcpy((*ppObject)->blob, pBlob, sz);
    rc = SQLITE_OK;
  }
  return rc;
}

static const int MOL_MAX_TXT_LENGTH = 2048;
static const int AS_SMILES = 0;
static const int AS_SMARTS = 1;

/*
** implementation for SMILES/SMARTS conversion to molecule object 
*/
static void cast_to_molecule(sqlite3_context* ctx, 
			     int argc, sqlite3_value** argv, int mode)
{
  assert(argc == 1);
  sqlite3_value *arg = argv[0];

  /* select the destination object type */
  u32 type = (mode == AS_SMILES) ? MOLOBJ : QMOLOBJ;

  /* build the molecule binary repr from a text string */
  if (sqlite3_value_type(arg) == SQLITE3_TEXT) {
    
    if (sqlite3_value_bytes(arg) > MOL_MAX_TXT_LENGTH) {
      sqlite3_result_error_toobig(ctx);
      return;
    }

    u8 *pBlob = 0;
    int sz = 0;
    int rc = txt_to_blob(sqlite3_value_text(arg), mode, &pBlob, &sz);
    if (rc != SQLITE_OK) {
      sqlite3_result_error_code(ctx, rc);
      return;
    }

    int objSz = 0;
    Object *pObject = 0;
    rc = wrap_object(pBlob, sz, type, &pObject, &objSz);
    if (rc == SQLITE_OK) {
      sqlite3_result_blob(ctx, pObject, objSz, sqlite3_free);
    }
    else {
      sqlite3_result_error_code(ctx, rc);
    }

    sqlite3_free(pBlob);
    
  }
  /* building a binary molecule repr from another binary blob */
  else if (sqlite3_value_type(arg) == SQLITE_BLOB) {
    
    int sz = sqlite3_value_bytes(arg);
    Object * pObject = (Object *)sqlite3_value_blob(arg);
    if (sz > sizeof(Object) && (OBJTYPE(pObject) == type)) {
      /* 
      ** the input argument is already a mol/qmol blob of the expected type
      ** just make a copy (almost a no-op)
      */
      u8 *pCopy = sqlite3_malloc(sz);
      if (pCopy) {
	memcpy(pCopy, pObject, sz);
	sqlite3_result_blob(ctx, pCopy, sz, sqlite3_free);
      }
      else {
	sqlite3_result_error_nomem(ctx);
      }
    }
    else {
      sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    }

  }
  /* neither a string nor a blob */
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
** fetch Mol from text or blob argument of molobj type
*/
static int fetch_mol_arg(sqlite3_value* arg, Mol **ppMol)
{
  int rc = SQLITE_MISMATCH;

  /* Check that value is a blob */
  if (sqlite3_value_type(arg) == SQLITE_BLOB) {
    int sz = sqlite3_value_bytes(arg);
    Object * pObj = (Object *)sqlite3_value_blob(arg);
    if ( (sz > sizeof(Object)) && 
	 (IS_MOLOBJ(pObj) || IS_QMOLOBJ(pObj)) ) {
      sz -= sizeof(Object);
      rc = blob_to_mol(pObj->blob, sz, ppMol);
    }
  }
  /* or a text string - by default assumed to be a SMILES */
  else if (sqlite3_value_type(arg) == SQLITE3_TEXT) {
    rc = sqlite3_value_bytes(arg) <= MOL_MAX_TXT_LENGTH ?
      txt_to_mol(sqlite3_value_text(arg), AS_SMILES, ppMol) : SQLITE_TOOBIG;
  }

  return rc;
}

/*
** convert molecule into a SMILES string
*/
static void mol_smiles_f(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  assert(argc == 1);
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

static void compare_structures(sqlite3_context* ctx, 
			       int argc, sqlite3_value** argv,
			       int (*compare)(Mol *, Mol*))
{
  assert(argc == 2);
  int rc = SQLITE_OK;

  Mol *p1 = 0;
  Mol *p2 = 0;
  
  rc = fetch_mol_arg(argv[0], &p1);
  if (rc != SQLITE_OK) goto mol_is_substruct_f_end;

  rc = fetch_mol_arg(argv[1], &p2);
  if (rc != SQLITE_OK) goto mol_is_substruct_f_free_mol1;

  int result = compare(p1, p2) ? 1 : 0;

 mol_is_substruct_f_free_mol2: free_mol(p2);
 mol_is_substruct_f_free_mol1: free_mol(p1);
 mol_is_substruct_f_end:
  if (rc == SQLITE_OK) {
    sqlite3_result_int(ctx, result);
  }
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

/*
** substructure match
*/
static void mol_is_substruct_f(sqlite3_context* ctx, 
			       int argc, sqlite3_value** argv)
{
  compare_structures(ctx, argc, argv, mol_is_substruct);
}

/* same as function above but with args swapped */
static int mol_substruct_of(Mol * p1, Mol * p2)
{
  return mol_is_substruct(p2, p1);
}

static void mol_substruct_of_f(sqlite3_context* ctx, 
			       int argc, sqlite3_value** argv)
{
  compare_structures(ctx, argc, argv, mol_substruct_of);
}

/*
** structural equality
*/
static int mol_same(Mol * p1, Mol * p2) {return mol_cmp(p1, p2) ? 0 : 1;}

static void mol_same_f(sqlite3_context* ctx, 
		       int argc, sqlite3_value** argv)
{
  compare_structures(ctx, argc, argv, mol_same);
}

/*
** molecular descriptors
*/

static void compute_real_descriptor(sqlite3_context* ctx, 
				    int argc, sqlite3_value** argv,
				    double (*descriptor)(Mol *))
{
  assert(argc == 1);

  Mol *pMol = 0;
  int rc = fetch_mol_arg(argv[0], &pMol);

  if (rc == SQLITE_OK) {
    double rd = descriptor(pMol);
    free_mol(pMol);
    sqlite3_result_double(ctx, rd);
  }
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

static void compute_int_descriptor(sqlite3_context* ctx, 
				   int argc, sqlite3_value** argv,
				   int (*descriptor)(Mol *))
{
  assert(argc == 1);

  Mol *pMol = 0;
  int rc = fetch_mol_arg(argv[0], &pMol);

  if (rc == SQLITE_OK) {
    int id = descriptor(pMol);
    free_mol(pMol);
    sqlite3_result_int(ctx, id);
  }
  else {
    sqlite3_result_error_code(ctx, rc);
  }
}

#define REAL_VALUED_MOL_DESCRIPTOR(func) \
static void func##_f(sqlite3_context* ctx, int argc, sqlite3_value** argv) \
{ \
  compute_real_descriptor(ctx, argc, argv, func); \
}

REAL_VALUED_MOL_DESCRIPTOR(mol_mw)
REAL_VALUED_MOL_DESCRIPTOR(mol_logp)
REAL_VALUED_MOL_DESCRIPTOR(mol_tpsa)

#define INT_VALUED_MOL_DESCRIPTOR(func)	 \
static void func##_f(sqlite3_context* ctx, int argc, sqlite3_value** argv) \
{ \
  compute_int_descriptor(ctx, argc, argv, func); \
}

INT_VALUED_MOL_DESCRIPTOR(mol_hba)
INT_VALUED_MOL_DESCRIPTOR(mol_hbd)
INT_VALUED_MOL_DESCRIPTOR(mol_num_atms)
INT_VALUED_MOL_DESCRIPTOR(mol_num_hvyatms)
INT_VALUED_MOL_DESCRIPTOR(mol_num_rotatable_bnds)
INT_VALUED_MOL_DESCRIPTOR(mol_num_hetatms)
INT_VALUED_MOL_DESCRIPTOR(mol_num_rings)

/*
** Register the chemicalite module with database handle db.
*/
int sqlite3_chemicalite_init(sqlite3 *db)
{
  int rc = SQLITE_OK;

#define CREATE_FUNCTION(argc, func) \
  do { \
    if (rc == SQLITE_OK) { \
      rc = sqlite3_create_function(db, # func, \
				   argc, SQLITE_UTF8, 0, func##_f, 0, 0); \
    } \
  } while (0)
  
#define CREATE_UNARY_FUNCTION(func) CREATE_FUNCTION(1, func)
#define CREATE_BINARY_FUNCTION(func) CREATE_FUNCTION(2, func)

  CREATE_UNARY_FUNCTION(mol);
  CREATE_UNARY_FUNCTION(qmol);

  CREATE_UNARY_FUNCTION(mol_smiles);
  
  CREATE_BINARY_FUNCTION(mol_is_substruct);
  CREATE_BINARY_FUNCTION(mol_substruct_of);
  CREATE_BINARY_FUNCTION(mol_same);
  
  CREATE_UNARY_FUNCTION(mol_mw);
  CREATE_UNARY_FUNCTION(mol_logp);
  CREATE_UNARY_FUNCTION(mol_tpsa);

  CREATE_UNARY_FUNCTION(mol_hba);
  CREATE_UNARY_FUNCTION(mol_hbd);
  CREATE_UNARY_FUNCTION(mol_num_atms);
  CREATE_UNARY_FUNCTION(mol_num_hvyatms);
  CREATE_UNARY_FUNCTION(mol_num_rotatable_bnds);
  CREATE_UNARY_FUNCTION(mol_num_hetatms);
  CREATE_UNARY_FUNCTION(mol_num_rings);
  
  return rc;
}

int sqlite3_extension_init(sqlite3 *db, char **pzErrMsg,
			   const sqlite3_api_routines *pApi)
{
  SQLITE_EXTENSION_INIT2(pApi)
  return sqlite3_chemicalite_init(db);
}
