#ifndef CHEMICALITE_RDKIT_ADAPTER_INCLUDED
#define CHEMICALITE_RDKIT_ADAPTER_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct CMol CMol;

  void free_cmol(CMol *pCMol);

  int txt_to_cmol(const char * txt, bool as_smarts, CMol **ppCMol);

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
