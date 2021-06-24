#ifndef CHEMICALITE_BINARY_FINGERPRINT_OPERATIONS_INCLUDED
#define CHEMICALITE_BINARY_FINGERPRINT_OPERATIONS_INCLUDED
#include <cstdint>

void bfp_op_union(int length, uint8_t *bfp1, const uint8_t *bfp2);
int bfp_op_weight(int length, const uint8_t *bfp);
int bfp_op_subset_weight(int length, const uint8_t *bfp, const uint8_t mask);
int bfp_op_growth(int length, const uint8_t *bfp1, const uint8_t *bfp2);
int bfp_op_iweight(int length, const uint8_t *bfp1, const uint8_t *bfp2);
int bfp_op_contains(int length, const uint8_t *bfp1, const uint8_t *bfp2);
int bfp_op_intersects(int length, const uint8_t *bfp1, const uint8_t *bfp2);
double bfp_op_tanimoto(int length, const uint8_t *bfp1, const uint8_t *bfp2);
double bfp_op_dice(int length, const uint8_t *bfp1, const uint8_t *bfp2);
int bfp_op_cmp(int length, const uint8_t *bfp1, const uint8_t *bfp2);

#endif
