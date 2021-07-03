
#include <cassert>

#include "bfp_ops.hpp"

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

int bfp_op_weight(int length, const uint8_t *bfp)
{
  int total_popcount = 0; 

  int ilength = length / sizeof(POPCNT_TYPE);

  POPCNT_TYPE * ibfp = (POPCNT_TYPE *) bfp;
  POPCNT_TYPE * ibfp_end = ibfp + ilength;
  POPCNT_TYPE * ibfp4_end = ibfp_end - (ilength % 4);
  
  while (ibfp < ibfp4_end) {
    total_popcount += POPCNT(*ibfp++);
    total_popcount += POPCNT(*ibfp++);
    total_popcount += POPCNT(*ibfp++);
    total_popcount += POPCNT(*ibfp++);
  }
  
  while (ibfp < ibfp_end) {
    total_popcount += POPCNT(*ibfp++);
  }
  
  const uint8_t * bfp_end = bfp + length;
  bfp = (const uint8_t *) ibfp;
  
  while (bfp < bfp_end) {
    total_popcount += byte_popcounts[*bfp++];
  }
  
  return total_popcount;
}

int bfp_op_subset_weight(int length, const uint8_t *bfp, const uint8_t byte_mask)
{
  int total_popcount = 0; 
  unsigned int i;

  POPCNT_TYPE mask = byte_mask;
  for (i = 1; i < sizeof(POPCNT_TYPE); ++i) {
    mask <<= 8;
    mask |= byte_mask;
  }
  
  int ilength = length / sizeof(POPCNT_TYPE);

  POPCNT_TYPE * ibfp = (POPCNT_TYPE *) bfp;
  POPCNT_TYPE * ibfp_end = ibfp + ilength;
  POPCNT_TYPE * ibfp4_end = ibfp_end - (ilength % 4);
  
  while (ibfp < ibfp4_end) {
    total_popcount += POPCNT(mask & *ibfp++);
    total_popcount += POPCNT(mask & *ibfp++);
    total_popcount += POPCNT(mask & *ibfp++);
    total_popcount += POPCNT(mask & *ibfp++);
  }

  while (ibfp < ibfp_end) {
    total_popcount += POPCNT(mask & *ibfp++);
  }
  
  const uint8_t * bfp_end = bfp + length;
  bfp = (const uint8_t *) ibfp;
  
  while (bfp < bfp_end) {
    total_popcount += byte_popcounts[byte_mask & *bfp++];
  }
  
  return total_popcount;
}

void bfp_op_union(int length, uint8_t *bfp1, const uint8_t *bfp2)
{
  int ilength = length / sizeof(POPCNT_TYPE);

  POPCNT_TYPE * ibfp1 = (POPCNT_TYPE *) bfp1;
  POPCNT_TYPE * ibfp2 = (POPCNT_TYPE *) bfp2;
  POPCNT_TYPE * ibfp1_end = ibfp1 + ilength;
  POPCNT_TYPE * ibfp4_end = ibfp1_end - (ilength % 4);
  
  while (ibfp1 < ibfp4_end) {
    *ibfp1++ |= *ibfp2++;
    *ibfp1++ |= *ibfp2++;
    *ibfp1++ |= *ibfp2++;
    *ibfp1++ |= *ibfp2++;
  }
  
  while (ibfp1 < ibfp1_end) {
    *ibfp1++ |= *ibfp2++;
  }
  
  uint8_t * bfp1_end = bfp1 + length;
  bfp1 = (uint8_t *) ibfp1;
  bfp2 = (const uint8_t *) ibfp2;
  
  while (bfp1 < bfp1_end) {
    *bfp1++ |= *bfp2++;
  }
}

int bfp_op_growth(int length, const uint8_t *bfp1, const uint8_t *bfp2)
{
  int growth = 0; 
  
  int ilength = length / sizeof(POPCNT_TYPE);

  POPCNT_TYPE ib1;
  POPCNT_TYPE * ibfp1 = (POPCNT_TYPE *) bfp1;
  POPCNT_TYPE * ibfp2 = (POPCNT_TYPE *) bfp2;
  POPCNT_TYPE * ibfp1_end = ibfp1 + ilength;
  POPCNT_TYPE * ibfp4_end = ibfp1_end - (ilength % 4);
  
  while (ibfp1 < ibfp4_end) {
    ib1 = *ibfp1++; growth += POPCNT(ib1 ^ (ib1 | *ibfp2++));
    ib1 = *ibfp1++; growth += POPCNT(ib1 ^ (ib1 | *ibfp2++));
    ib1 = *ibfp1++; growth += POPCNT(ib1 ^ (ib1 | *ibfp2++));
    ib1 = *ibfp1++; growth += POPCNT(ib1 ^ (ib1 | *ibfp2++));
  }
  
  while (ibfp1 < ibfp1_end) {
    ib1 = *ibfp1++; growth += POPCNT(ib1 ^ (ib1 | *ibfp2++));
  }
  
  const uint8_t * bfp1_end = bfp1 + length;
  bfp1 = (const uint8_t *) ibfp1;
  bfp2 = (const uint8_t *) ibfp2;
  
  while (bfp1 < bfp1_end) {
    const uint8_t b1 = *bfp1++; 
    const uint8_t b2 = *bfp2++;
    growth += byte_popcounts[b1 ^ (b1 | b2)];
  }

  return growth;
}

int bfp_op_iweight(int length, const uint8_t *bfp1, const uint8_t *bfp2)
{
  int intersect_popcount = 0;

  int ilength = length / sizeof(POPCNT_TYPE);

  POPCNT_TYPE * ibfp1 = (POPCNT_TYPE *) bfp1;
  POPCNT_TYPE * ibfp2 = (POPCNT_TYPE *) bfp2;
  POPCNT_TYPE * ibfp1_end = ibfp1 + ilength;
  POPCNT_TYPE * ibfp4_end = ibfp1_end - (ilength % 4);
  
  while (ibfp1 < ibfp4_end) {
    intersect_popcount += POPCNT(*ibfp1++ & *ibfp2++);
    intersect_popcount += POPCNT(*ibfp1++ & *ibfp2++);
    intersect_popcount += POPCNT(*ibfp1++ & *ibfp2++);
    intersect_popcount += POPCNT(*ibfp1++ & *ibfp2++);
  }
  
  while (ibfp1 < ibfp1_end) {
    intersect_popcount += POPCNT(*ibfp1++ & *ibfp2++);
  }
  
  const uint8_t * bfp1_end = bfp1 + length;
  bfp1 = (const uint8_t *) ibfp1;
  bfp2 = (const uint8_t *) ibfp2;
  
  while (bfp1 < bfp1_end) {
    intersect_popcount += byte_popcounts[*bfp1++ & *bfp2++];
  }
  
  return intersect_popcount;
}

int bfp_op_contains(int length, const uint8_t *bfp1, const uint8_t *bfp2)
{
  int contains = 1;

  int ilength = length / sizeof(POPCNT_TYPE);

  POPCNT_TYPE * ibfp1 = (POPCNT_TYPE *) bfp1;
  POPCNT_TYPE * ibfp2 = (POPCNT_TYPE *) bfp2;
  POPCNT_TYPE * ibfp1_end = ibfp1 + ilength;
  
  while (contains && ibfp1 < ibfp1_end) {
    POPCNT_TYPE i1 = *ibfp1++;
    POPCNT_TYPE i2 = *ibfp2++;
    contains = i1 == (i1 | i2);
  }
  
  const uint8_t * bfp1_end = bfp1 + length;
  bfp1 = (const uint8_t *) ibfp1;
  bfp2 = (const uint8_t *) ibfp2;
  
  while (contains && bfp1 < bfp1_end) {
    const uint8_t b1 = *bfp1++; 
    const uint8_t b2 = *bfp2++;
    contains = b1 == (b1 | b2);
  }

  return contains;
}

int bfp_op_intersects(int length, const uint8_t *bfp1, const uint8_t *bfp2)
{
  int intersects = 0;

  int ilength = length / sizeof(POPCNT_TYPE);

  POPCNT_TYPE * ibfp1 = (POPCNT_TYPE *) bfp1;
  POPCNT_TYPE * ibfp2 = (POPCNT_TYPE *) bfp2;
  POPCNT_TYPE * ibfp1_end = ibfp1 + ilength;
  
  while (!intersects && ibfp1 < ibfp1_end) {
    POPCNT_TYPE i1 = *ibfp1++;
    POPCNT_TYPE i2 = *ibfp2++;
    intersects = i1 & i2;
  }
  
  const uint8_t * bfp1_end = bfp1 + length;
  bfp1 = (const uint8_t *) ibfp1;
  bfp2 = (const uint8_t *) ibfp2;
  
  while (!intersects && bfp1 < bfp1_end) {
    const uint8_t b1 = *bfp1++; 
    const uint8_t b2 = *bfp2++;
    intersects = b1 & b2;
  }

  return intersects;
}

double bfp_op_tanimoto(int length, const uint8_t *afp, const uint8_t *bfp)
{
  double sim;

  // Nsame / (Na + Nb - Nsame)
  int union_popcount = 0;
  int intersect_popcount = 0;

  int ilength = length / sizeof(POPCNT_TYPE);

  POPCNT_TYPE * iafp = (POPCNT_TYPE *) afp;
  POPCNT_TYPE * ibfp = (POPCNT_TYPE *) bfp;
  POPCNT_TYPE * iafp_end = iafp + ilength;
  POPCNT_TYPE * iafp4_end = iafp_end - (ilength % 4);

  POPCNT_TYPE ia, ib;
  while (iafp < iafp4_end) {
    ia = *iafp++; ib = *ibfp++;
    union_popcount += POPCNT(ia | ib); intersect_popcount += POPCNT(ia & ib);
    ia = *iafp++; ib = *ibfp++;
    union_popcount += POPCNT(ia | ib); intersect_popcount += POPCNT(ia & ib);
    ia = *iafp++; ib = *ibfp++;
    union_popcount += POPCNT(ia | ib); intersect_popcount += POPCNT(ia & ib);
    ia = *iafp++; ib = *ibfp++;
    union_popcount += POPCNT(ia | ib); intersect_popcount += POPCNT(ia & ib);
  }
  
  while (iafp < iafp_end) {
    ia = *iafp++; ib = *ibfp++;
    union_popcount += POPCNT(ia | ib); intersect_popcount += POPCNT(ia & ib);
  }
  
  const uint8_t * afp_end = afp + length;
  afp = (const uint8_t *) iafp;
  bfp = (const uint8_t *) ibfp;
  
  while (afp < afp_end) {
    const uint8_t ba = *afp++;
    const uint8_t bb = *bfp++;
    union_popcount += byte_popcounts[ ba | bb ];
    intersect_popcount += byte_popcounts[ ba & bb ];
  }
    
  if (union_popcount != 0) {
    sim = ((double)intersect_popcount) / union_popcount;
  }
  else {
    sim = 1.0;
  }
  
  return sim;
}

double bfp_op_dice(int length, const uint8_t *afp, const uint8_t *bfp)
{
  double sim = 0.0;
  
  // 2 * Nsame / (Na + Nb)
  int intersect_popcount = 0;
  int total_popcount = 0; 
  
  int ilength = length / sizeof(POPCNT_TYPE);

  POPCNT_TYPE * iafp = (POPCNT_TYPE *) afp;
  POPCNT_TYPE * ibfp = (POPCNT_TYPE *) bfp;
  POPCNT_TYPE * iafp_end = iafp + ilength;
  POPCNT_TYPE * iafp4_end = iafp_end - (ilength % 4);

  POPCNT_TYPE ia, ib;
  while (iafp < iafp4_end) {
    ia = *iafp++; ib = *ibfp++;
    total_popcount += POPCNT(ia) + POPCNT(ib);
    intersect_popcount += POPCNT(ia & ib);
    ia = *iafp++; ib = *ibfp++;
    total_popcount += POPCNT(ia) + POPCNT(ib);
    intersect_popcount += POPCNT(ia & ib);
    ia = *iafp++; ib = *ibfp++;
    total_popcount += POPCNT(ia) + POPCNT(ib);
    intersect_popcount += POPCNT(ia & ib);
    ia = *iafp++; ib = *ibfp++;
    total_popcount += POPCNT(ia) + POPCNT(ib);
    intersect_popcount += POPCNT(ia & ib);
  }
  
  while (iafp < iafp_end) {
    ia = *iafp++; ib = *ibfp++;
    total_popcount += POPCNT(ia) + POPCNT(ib);
    intersect_popcount += POPCNT(ia & ib);
  }
  
  const uint8_t * afp_end = afp + length;
  afp = (const uint8_t *) iafp;
  bfp = (const uint8_t *) ibfp;
  
  while (afp < afp_end) {
    const uint8_t ba = *afp++;
    const uint8_t bb = *bfp++;
    total_popcount += byte_popcounts[ba] + byte_popcounts[bb];
    intersect_popcount += byte_popcounts[ba & bb];
  }

  if (total_popcount != 0) {
    sim = (2.0 * intersect_popcount) / (total_popcount);
  }

  return sim;
}

int bfp_op_cmp(int length, const uint8_t *afp, const uint8_t *bfp)
{
  uint8_t higher = 1;

  const uint8_t *afp_end = afp + length;

  while (afp < afp_end) {
    const uint8_t bytea = *afp++;
    const uint8_t byteb = *bfp++;
    if (bytea == byteb) {
      // if the number of 1s in ba is odd, higher needs to be flipped
      higher ^= (1 & byte_popcounts[bytea]);
    }
    else {
      uint8_t mask = 0x80;
      while (mask) {
        uint8_t bita = (bytea & mask) ? 1 : 0;
        uint8_t bitb = (byteb & mask) ? 1 : 0;
        if (bita != bitb) {
          return (bita == higher) ? 1 : -1;
        }
        else {
          // flip higher if bita is 1
          higher ^= bita;
        }
        mask >>= 1;
      }
      assert(!"should never get here if bytea != byteb");
    }
  }

  // same bfp value
  return 0;
}
