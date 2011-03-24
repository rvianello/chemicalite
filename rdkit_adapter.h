#ifndef CHEMICALITE_RDKIT_ADAPTER_INCLUDED
#define CHEMICALITE_RDKIT_ADAPTER_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct CMol CMol;
  typedef unsigned char u8;
  typedef unsigned int u32;


  void free_cmol(CMol *pCMol);

  int txt_to_cmol(const char * txt, bool as_smarts, CMol **ppCMol);
  int cmol_to_txt(CMol *pCMol, bool as_smarts, char **pTxt);

  int blob_to_cmol(u8 *blob, int len, CMol **ppCMol);
  int cmol_to_blob(CMol *pCMol, u8 **pBlob, int *pLen);

  double cmol_amw(CMol *pCMol);
  double cmol_logp(CMol *pCMol);
  double cmol_tpsa(CMol *pCMol);
  
  int cmol_hba(CMol *pCMol);
  int cmol_hbd(CMol *pCMol);
  int cmol_num_atms(CMol *pCMol);
  int cmol_num_hvyatms(CMol *pCMol);
  int cmol_num_rotatable_bnds(CMol *pCMol);
  int cmol_num_hetatms(CMol *pCMol);
  int cmol_num_rings(CMol *pCMol);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif
