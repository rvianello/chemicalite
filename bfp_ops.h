#ifndef CHEMICALITE_BINARY_FINGERPRINT_OPERATIONS_INCLUDED
#define CHEMICALITE_BINARY_FINGERPRINT_OPERATIONS_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

  void bfp_op_union(int length, u8 *bfp1, u8 *bfp2);
  int bfp_op_weight(int length, u8 *bfp);
  int bfp_op_growth(int length, u8 *bfp1, u8 *bfp2);
  int bfp_op_iweight(int length, u8 *bfp1, u8 *bfp2);
  int bfp_op_contains(int length, u8 *bfp1, u8 *bfp2);
  double bfp_op_tanimoto(int length, u8 *bfp1, u8 *bfp2);
  double bfp_op_dice(int length, u8 *bfp1, u8 *bfp2);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif
