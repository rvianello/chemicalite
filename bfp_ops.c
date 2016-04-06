#include "chemicalite.h"
#include "bfp_ops.h"

// the Tanimoto and Dice similarity code is adapted
// from Gred Landrum's RDKit PostgreSQL cartridge code that in turn is
// adapted from Andrew Dalke's chem-fingerprints code
// http://code.google.com/p/chem-fingerprints/

#ifdef __MSC_VER
#include <intrin.h>
#define POPCNT __popcnt
typedef unsigned int POPCNT_TYPE;
#else
#define POPCNT __builtin_popcountll
typedef unsigned long long POPCNT_TYPE;
#endif

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
  int i, length2;

  POPCNT_TYPE * ibfp = (POPCNT_TYPE *) bfp;
  int ilength = length / sizeof(POPCNT_TYPE);
  POPCNT_TYPE * ibfp_end = ibfp + ilength;
  while (ibfp < ibfp_end) {
    total_popcount += POPCNT(*ibfp++);
  }
  if (length % sizeof(POPCNT_TYPE)) {
    length2 = ilength * sizeof(POPCNT_TYPE);
    bfp += length2;
    for (i = length2; i < length; ++i) {
      total_popcount += byte_popcounts[*bfp++];
    }
  }
  return total_popcount;
}

int bfp_op_subset_weight(int length, u8 *bfp, u8 byte_mask)
{
  int total_popcount = 0; 
  int i, length2;

  POPCNT_TYPE mask = byte_mask;
  for (i = 1; i < sizeof(POPCNT_TYPE); ++i) {
    mask <<= 8;
    mask |= byte_mask;
  }
  
  POPCNT_TYPE * ibfp = (POPCNT_TYPE *) bfp;
  int ilength = length / sizeof(POPCNT_TYPE);
  POPCNT_TYPE * ibfp_end = ibfp + ilength;
  while (ibfp < ibfp_end) {
    total_popcount += POPCNT(mask & *ibfp++);
  }
  if (length % sizeof(POPCNT_TYPE)) {
    length2 = ilength * sizeof(POPCNT_TYPE);
    bfp += length2;
    for (i = length2; i < length; ++i) {
      total_popcount += byte_popcounts[byte_mask & *bfp++];
    }
  }
  return total_popcount;
}

void bfp_op_union(int length, u8 *bfp1, u8 *bfp2)
{
  int i, length2;
  POPCNT_TYPE * ibfp1 = (POPCNT_TYPE *) bfp1;
  POPCNT_TYPE * ibfp2 = (POPCNT_TYPE *) bfp2;
  int ilength = length / sizeof(POPCNT_TYPE);
  POPCNT_TYPE * ibfp1_end = ibfp1 + ilength;
  while (ibfp1 < ibfp1_end) {
    *ibfp1++ |= *ibfp2++;
  }
  if (length % sizeof(POPCNT_TYPE)) {
    length2 = ilength * sizeof(POPCNT_TYPE);
    bfp1 += length2;
    bfp2 += length2;
    for (i = length2; i < length; ++i) {
      *bfp1++ |= *bfp2++;
    }
  }
}

int bfp_op_growth(int length, u8 *bfp1, u8 *bfp2)
{
  int growth = 0; 
  int i, length2;
  POPCNT_TYPE * ibfp1 = (POPCNT_TYPE *) bfp1;
  POPCNT_TYPE * ibfp2 = (POPCNT_TYPE *) bfp2;
  int ilength = length / sizeof(POPCNT_TYPE);
  POPCNT_TYPE * ibfp1_end = ibfp1 + ilength;
  while (ibfp1 < ibfp1_end) {
    POPCNT_TYPE i1 = *ibfp1++;
    POPCNT_TYPE i2 = *ibfp2++;
    growth += POPCNT(i1 ^ (i1 | i2));
  }
  if (length % sizeof(POPCNT_TYPE)) {
    length2 = ilength * sizeof(POPCNT_TYPE);
    bfp1 += length2;
    bfp2 += length2;
    for (i = length2; i < length; ++i) {
      u8 b1 = *bfp1++; 
      u8 b2 = *bfp2++;
      growth += byte_popcounts[b1 ^ (b1 | b2)];
    }
  }
  return growth;
}

int bfp_op_iweight(int length, u8 *bfp1, u8 *bfp2)
{
  int intersect_popcount = 0;
  int i, length2;
  POPCNT_TYPE * ibfp1 = (POPCNT_TYPE *) bfp1;
  POPCNT_TYPE * ibfp2 = (POPCNT_TYPE *) bfp2;
  int ilength = length / sizeof(POPCNT_TYPE);
  POPCNT_TYPE * ibfp1_end = ibfp1 + ilength;
  while (ibfp1 < ibfp1_end) {
    intersect_popcount += POPCNT(*ibfp1++ & *ibfp2++);
  }
  if (length % sizeof(POPCNT_TYPE)) {
    length2 = ilength * sizeof(POPCNT_TYPE);
    bfp1 += length2;
    bfp2 += length2;
    for (i = length2; i < length; ++i) {
      intersect_popcount += byte_popcounts[*bfp1++ & *bfp2++];
    }
  }
  return intersect_popcount;
}

int bfp_op_contains(int length, u8 *bfp1, u8 *bfp2)
{
  int contains = 1;
  int i, length2;
  POPCNT_TYPE * ibfp1 = (POPCNT_TYPE *) bfp1;
  POPCNT_TYPE * ibfp2 = (POPCNT_TYPE *) bfp2;
  int ilength = length / sizeof(POPCNT_TYPE);
  POPCNT_TYPE * ibfp1_end = ibfp1 + ilength;
  while (contains && ibfp1 < ibfp1_end) {
    POPCNT_TYPE i1 = *ibfp1++;
    POPCNT_TYPE i2 = *ibfp2++;
    contains = i1 == (i1 | i2);
  }
  if (contains && length % sizeof(POPCNT_TYPE)) {
    length2 = ilength * sizeof(POPCNT_TYPE);
    bfp1 += length2;
    bfp2 += length2;
    for (i = length2; contains && i < length; ++i) {
      u8 b1 = *bfp1++; 
      u8 b2 = *bfp2++;
      contains = b1 == (b1 | b2);
    }
  }
  return contains;
}

double bfp_op_tanimoto(int length, u8 *afp, u8 *bfp)
{
  double sim;

  // Nsame / (Na + Nb - Nsame)
  int union_popcount = 0;
  int intersect_popcount = 0;
  int i, length2;

  POPCNT_TYPE * iafp = (POPCNT_TYPE *) afp;
  POPCNT_TYPE * ibfp = (POPCNT_TYPE *) bfp;
  int ilength = length / sizeof(POPCNT_TYPE);
  POPCNT_TYPE * iafp_end = iafp + ilength;
  while (iafp < iafp_end) {
    POPCNT_TYPE ia = *iafp++;
    POPCNT_TYPE ib = *ibfp++;
    union_popcount += POPCNT(ia | ib);
    intersect_popcount += POPCNT(ia & ib);
  }
  if (length % sizeof(POPCNT_TYPE)) {
    length2 = ilength * sizeof(POPCNT_TYPE);
    afp += length2;
    bfp += length2;
    for (i = length2; i < length; ++i) {
      u8 ba = *afp++;
      u8 bb = *bfp++;
      union_popcount += byte_popcounts[ ba | bb ];
      intersect_popcount += byte_popcounts[ ba & bb ];
    }
  }
    
  if (union_popcount != 0) {
    sim = ((double)intersect_popcount) / union_popcount;
  }
  else {
    sim = 1.0;
  }
  
  return sim;
}

double bfp_op_dice(int length, u8 *afp, u8 *bfp)
{
  double sim = 0.0;
  
  // 2 * Nsame / (Na + Nb)
  int intersect_popcount = 0;
  int total_popcount = 0; 
  int i, length2;
  POPCNT_TYPE * iafp = (POPCNT_TYPE *) afp;
  POPCNT_TYPE * ibfp = (POPCNT_TYPE *) bfp;
  int ilength = length / sizeof(POPCNT_TYPE);
  POPCNT_TYPE * iafp_end = iafp + ilength;
  while (iafp < iafp_end) {
    POPCNT_TYPE ia = *iafp++;
    POPCNT_TYPE ib = *ibfp++;
    total_popcount += POPCNT(ia) + POPCNT(ib);
    intersect_popcount += POPCNT(ia & ib);
  }
  if (length % sizeof(POPCNT_TYPE)) {
    length2 = ilength * sizeof(POPCNT_TYPE);
    afp += length2;
    bfp += length2;
    for (i = length2; i < length; ++i) {
      u8 ba = *afp++;
      u8 bb = *bfp++;
      total_popcount += byte_popcounts[ba] + byte_popcounts[bb];
      intersect_popcount += byte_popcounts[ba & bb];
    }
  }

  if (total_popcount != 0) {
    sim = (2.0 * intersect_popcount) / (total_popcount);
  }

  return sim;
}
