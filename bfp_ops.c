
#include "chemicalite.h"
#include "bfp_ops.h"

// the Tanimoto and Dice similarity code is adapted
// from Gred Landrum's RDKit PostgreSQL cartridge code that in turn is
// adapted from Andrew Dalke's chem-fingerprints code
// http://code.google.com/p/chem-fingerprints/

static int byte_popcounts[] = {
  0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
  1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
  1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
  2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
  1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
  2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
  2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
  3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8  
};

int bfp_op_weight(int length, u8 *bfp)
{
  int total_popcount = 0; 
  int i;
  for (i = 0; i < length; ++i) {
    total_popcount += byte_popcounts[*bfp++];
  }
  return total_popcount;
}

void bfp_op_union(int length, u8 *bfp1, u8 *bfp2)
{
  int i;
  for (i = 0; i < length; ++i) {
    *bfp1++ |= *bfp2++;
  }
}

int bfp_op_growth(int length, u8 *bfp1, u8 *bfp2)
{
  int growth = 0; 
  int i;
  for (i = 0; i < length; ++i) {
    u8 b1 = *bfp1++; 
    u8 b2 = *bfp2++;
    growth += byte_popcounts[b1 ^ (b1 | b2)];
  }
  return growth;
}

int bfp_op_same(int length, u8 *bfp1, u8 *bfp2)
{
  int intersect_popcount = 0;
  int i;
  for (i = 0; i < length; ++i, ++bfp1, ++bfp2) {
    intersect_popcount += byte_popcounts[ *bfp1 & *bfp2 ];
  }
  return intersect_popcount;
}

int bfp_op_contains(int length, u8 *bfp1, u8 *bfp2)
{
  int contains = 1;
  int i;
  for (i = 0; contains && i < length; ++i) {
    u8 b1 = *bfp1++; 
    u8 b2 = *bfp2++;
    contains = b1 == (b1 | b2);
  }
  return contains;
}

double bfp_op_tanimoto(int length, u8 *afp, u8 *bfp)
{
  double sim = 0.0;

  // Nsame / (Na + Nb - Nsame)
  int union_popcount = 0;
  int intersect_popcount = 0;
  int i;
  for (i = 0; i < length; ++i, ++afp, ++bfp) {
    union_popcount += byte_popcounts[ *afp | *bfp ];
    intersect_popcount += byte_popcounts[ *afp & *bfp ];
  }
  
  if (union_popcount != 0) {
    sim = (intersect_popcount + 0.0) / union_popcount;
  }

  return sim;
}

double bfp_op_dice(int length, u8 *afp, u8 *bfp)
{
  double sim = 0.0;
  
  // 2 * Nsame / (Na + Nb)
  int intersect_popcount = 0;
  int total_popcount = 0; 
  int i;
  for (i = 0; i < length; ++i, ++afp, ++bfp) {
    total_popcount += byte_popcounts[*afp] + byte_popcounts[*bfp];
    intersect_popcount += byte_popcounts[*afp & *bfp];
  }

  if (total_popcount != 0) {
    sim = (2.0 * intersect_popcount) / (total_popcount);
  }

  return sim;
}
