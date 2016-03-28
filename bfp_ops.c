#include "chemicalite.h"
#include "bfp_ops.h"

// the Tanimoto and Dice similarity code is adapted
// from Gred Landrum's RDKit PostgreSQL cartridge code that in turn is
// adapted from Andrew Dalke's chem-fingerprints code
// http://code.google.com/p/chem-fingerprints/

#ifdef USE_BUILTIN_POPCOUNT

#ifdef __MSC_VER
#include <intrin.h>
#define __builtin_popcount __popcnt
#endif

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
  int i;
#ifdef USE_BUILTIN_POPCOUNT
  unsigned int * ibfp = (unsigned int *) bfp;
  int ilength = length / sizeof(unsigned int);
  for (i = 0; i < ilength; ++i) {
    total_popcount += __builtin_popcount(*ibfp++);
  }
  bfp += ilength * sizeof(unsigned int);
  for (i = ilength * sizeof(unsigned int); i < length; ++i) {
#else
  for (i = 0; i < length; ++i) {
#endif
    total_popcount += byte_popcounts[*bfp++];
  }
  return total_popcount;
}

void bfp_op_union(int length, u8 *bfp1, u8 *bfp2)
{
  int i;
#if 1
  unsigned int * ibfp1 = (unsigned int *) bfp1;
  unsigned int * ibfp2 = (unsigned int *) bfp2;
  int ilength = length / sizeof(unsigned int);
  for (i = 0; i < ilength; ++i) {
    *ibfp1++ |= *ibfp2++;
  }
  bfp1 += ilength * sizeof(unsigned int);
  bfp2 += ilength * sizeof(unsigned int);
  for (i = ilength * sizeof(unsigned int); i < length; ++i) {
#else
  for (i = 0; i < length; ++i) {
#endif
    *bfp1++ |= *bfp2++;
  }
}

int bfp_op_growth(int length, u8 *bfp1, u8 *bfp2)
{
  int growth = 0; 
  int i;
#ifdef USE_BUILTIN_POPCOUNT
  unsigned int * ibfp1 = (unsigned int *) bfp1;
  unsigned int * ibfp2 = (unsigned int *) bfp2;
  int ilength = length / sizeof(unsigned int);
  for (i = 0; i < ilength; ++i) {
    unsigned int i1 = *ibfp1++;
    unsigned int i2 = *ibfp2++;
    growth += __builtin_popcount(i1 ^ (i1 | i2));
  }
  bfp1 += ilength * sizeof(unsigned int);
  bfp2 += ilength * sizeof(unsigned int);
  for (i = ilength * sizeof(unsigned int); i < length; ++i) {
#else
  for (i = 0; i < length; ++i) {
#endif
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
#ifdef USE_BUILTIN_POPCOUNT
  unsigned int * ibfp1 = (unsigned int *) bfp1;
  unsigned int * ibfp2 = (unsigned int *) bfp2;
  int ilength = length / sizeof(unsigned int);
  for (i = 0; i < ilength; ++i) {
    intersect_popcount += __builtin_popcount(*ibfp1++ & *ibfp2++);
  }
  bfp1 += ilength * sizeof(unsigned int);
  bfp2 += ilength * sizeof(unsigned int);
  for (i = ilength * sizeof(unsigned int); i < length; ++i) {
#else
  for (i = 0; i < length; ++i) {
#endif
    intersect_popcount += byte_popcounts[*bfp1++ & *bfp2++];
  }
  return intersect_popcount;
}

int bfp_op_contains(int length, u8 *bfp1, u8 *bfp2)
{
  int contains = 1;
  int i;
#if 1
  unsigned int * ibfp1 = (unsigned int *) bfp1;
  unsigned int * ibfp2 = (unsigned int *) bfp2;
  int ilength = length / sizeof(unsigned int);
  for (i = 0; contains && i < ilength; ++i) {
    unsigned int i1 = *ibfp1++;
    unsigned int i2 = *ibfp2++;
    contains = i1 == (i1 | i2);
  }
  bfp1 += ilength * sizeof(unsigned int);
  bfp2 += ilength * sizeof(unsigned int);
  for (i = ilength * sizeof(unsigned int); contains && i < length; ++i) {
#else
  for (i = 0; contains && i < length; ++i) {
#endif
    u8 b1 = *bfp1++; 
    u8 b2 = *bfp2++;
    contains = b1 == (b1 | b2);
  }
  return contains;
}

double bfp_op_tanimoto(int length, u8 *afp, u8 *bfp)
{
  double sim;

  // Nsame / (Na + Nb - Nsame)
  int union_popcount = 0;
  int intersect_popcount = 0;
  int i;

#ifdef USE_BUILTIN_POPCOUNT
  unsigned int * iafp = (unsigned int *) afp;
  unsigned int * ibfp = (unsigned int *) bfp;
  int ilength = length / sizeof(unsigned int);
  for (i = 0; i < ilength; ++i) {
    unsigned int ia = *iafp++;
    unsigned int ib = *ibfp++;
    union_popcount += __builtin_popcount(ia | ib);
    intersect_popcount += __builtin_popcount(ia & ib);
  }
  if (length % sizeof(unsigned int)) {
    int ilength_end = ilength * sizeof(unsigned int);
    if (ilength_end < length) {
      afp += ilength_end;
      bfp += ilength_end;
    }
    for (i = ilength_end; i < length; ++i) {
      u8 ba = *afp++;
      u8 bb = *bfp++;
      union_popcount += byte_popcounts[ ba | bb ];
      intersect_popcount += byte_popcounts[ ba & bb ];
    }
  }
#else
  for (i = 0; i < length; ++i) {
    u8 ba = *afp++;
    u8 bb = *bfp++;
    union_popcount += byte_popcounts[ ba | bb ];
    intersect_popcount += byte_popcounts[ ba & bb ];
  }
#endif
    
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
  int i;
#ifdef USE_BUILTIN_POPCOUNT
  unsigned int * iafp = (unsigned int *) afp;
  unsigned int * ibfp = (unsigned int *) bfp;
  int ilength = length / sizeof(unsigned int);
  for (i = 0; i < ilength; ++i) {
    unsigned int ia = *iafp++;
    unsigned int ib = *ibfp++;
    total_popcount += __builtin_popcount(ia) + __builtin_popcount(ib);
    intersect_popcount += __builtin_popcount(ia & ib);
  }
  afp += ilength * sizeof(unsigned int);
  bfp += ilength * sizeof(unsigned int);
  for (i = ilength * sizeof(unsigned int); i < length; ++i) {
#else
  for (i = 0; i < length; ++i) {
#endif
    u8 ba = *afp++;
    u8 bb = *bfp++;
    total_popcount += byte_popcounts[ba] + byte_popcounts[bb];
    intersect_popcount += byte_popcounts[ba & bb];
  }

  if (total_popcount != 0) {
    sim = (2.0 * intersect_popcount) / (total_popcount);
  }

  return sim;
}
