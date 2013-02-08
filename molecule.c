#ifndef CHEMICALITE_MOLECULE_INCLUDED
#define CHEMICALITE_MOLECULE_INCLUDED

#include "rdkit_adapter.h"

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
  if (rc != SQLITE_OK) goto compare_f_end;

  rc = fetch_mol_arg(argv[1], &p2);
  if (rc != SQLITE_OK) goto compare_f_free_mol1;

  int result = compare(p1, p2);

 compare_f_free_mol2: free_mol(p2);
 compare_f_free_mol1: free_mol(p1);
 compare_f_end:
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

static void mol_is_superstruct_f(sqlite3_context* ctx, 
				 int argc, sqlite3_value** argv)
{
  compare_structures(ctx, argc, argv, mol_is_superstruct);
}

/*
** structural comparison (ordering)
*/
static void mol_cmp_f(sqlite3_context* ctx, 
		       int argc, sqlite3_value** argv)
{
  compare_structures(ctx, argc, argv, mol_cmp);
}

/*
** molecular descriptors
*/

#define MOL_DESCRIPTOR(func, type) \
  static void func##_f(sqlite3_context* ctx, int argc, sqlite3_value** argv) \
  {									\
    assert(argc == 1);							\
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

#else
#error "module module included multiple times"
#endif
