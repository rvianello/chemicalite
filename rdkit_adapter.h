#ifndef CHEMICALITE_RDKIT_ADAPTER_INCLUDED
#define CHEMICALITE_RDKIT_ADAPTER_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif
  void free_mol(Mol *pMol);
  void free_bfp(Bfp *pBfp);

  /* molecular data types interconversion */
  int txt_to_mol(const char * txt, int as_smarts, Mol **ppMol);
  int mol_to_txt(Mol *pMol, int as_smarts, char **pTxt);

  int blob_to_mol(const u8 *blob, int len, Mol **ppMol);
  int mol_to_blob(Mol *pMol, u8 **ppBlob, int *pLen);

  int txt_to_blob(const char * txt, int as_smarts, u8 **pBlob, int *pLen);
  int blob_to_txt(u8 *blob, int len, int as_smarts, char **pTxt);

  /* mol ops */
  int mol_cmp(Mol *p1, Mol *p2);
  int mol_is_substruct(Mol *p1, Mol *p2);
  int mol_is_superstruct(Mol *p1, Mol *p2);

  /* descriptors */
  double mol_mw(Mol *pMol);
  double mol_logp(Mol *pMol);
  double mol_tpsa(Mol *pMol);
  double mol_chi0v(Mol *pMol);
  double mol_chi1v(Mol *pMol);
  double mol_chi2v(Mol *pMol);
  double mol_chi3v(Mol *pMol);
  double mol_chi4v(Mol *pMol);
  double mol_chi0n(Mol *pMol);
  double mol_chi1n(Mol *pMol);
  double mol_chi2n(Mol *pMol);
  double mol_chi3n(Mol *pMol);
  double mol_chi4n(Mol *pMol);
  double mol_kappa1(Mol *pMol);
  double mol_kappa2(Mol *pMol);
  double mol_kappa3(Mol *pMol);
  
  int mol_hba(Mol *pMol);
  int mol_hbd(Mol *pMol);
  int mol_num_atms(Mol *pMol);
  int mol_num_hvyatms(Mol *pMol);
  int mol_num_rotatable_bnds(Mol *pMol);
  int mol_num_hetatms(Mol *pMol);
  int mol_num_rings(Mol *pMol);

  /* bfp data type interconversion */
  int bfp_to_blob(Bfp *pBfp, u8 **ppBlob, int *pLen);
  int blob_to_bfp(const u8 *pBlob, int len, Bfp **ppBfp);

  /* bfp ops */
  double bfp_tanimoto(Bfp *pBfp1, Bfp *pBfp2);
  double bfp_dice(Bfp *pBfp1, Bfp *pBfp2);

  int bfp_length(Bfp *pBfp);
  int bfp_weight(Bfp *pBfp);

  /* mol -> bfp */
  int mol_layered_bfp(Mol *pMol, Bfp **ppBfp);
  int mol_rdkit_bfp(Mol *pMol, Bfp **ppBfp);
  int mol_morgan_bfp(Mol *pMol, int radius, Bfp **ppBfp);
  int mol_feat_morgan_bfp(Mol *pMol, int radius, Bfp **ppBfp);
  int mol_atom_pairs_bfp(Mol *pMol, Bfp **ppBfp);
  int mol_topological_torsion_bfp(Mol *pMol, Bfp **ppBfp);
  int mol_maccs_bfp(Mol *pMol, Bfp **ppBfp);

  /* generation of structural signature */
  int mol_bfp_signature(Mol *pMol, Bfp **ppBfp);
  
#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif
