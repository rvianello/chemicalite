#ifndef CHEMICALITE_RDKIT_ADAPTER_INCLUDED
#define CHEMICALITE_RDKIT_ADAPTER_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct CMol CMol;

  void free_cmol(CMol *);

  double cmol_amw(CMol *);
  double cmol_logp(CMol *);
  double cmol_tpsa(CMol *);
  
  int cmol_hba(CMol *);
  int cmol_hbd(CMol *);
  int cmol_num_atms(CMol *);
  int cmol_num_hvyatms(CMol *);
  int cmol_num_rotatable_bnds(CMol *);
  int cmol_num_hetatms(CMol *);
  int cmol_num_rings(CMol *);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif
