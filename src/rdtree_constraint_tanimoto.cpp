#include <cassert>
#include <cmath>
#include <algorithm>

#include "rdtree_vtab.hpp"
#include "rdtree_constraint_tanimoto.hpp"
#include "rdtree_item.hpp"
#include "bfp_ops.hpp"

std::shared_ptr<RDtreeConstraint> RDtreeTanimoto::deserialize(const uint8_t * data, int size, const RDtreeVtab & vtab, int * rc)
{
  std::shared_ptr<RDtreeConstraint> result;

  if (size != (vtab.bfp_bytes + (int)sizeof(double))) {
    *rc = SQLITE_MISMATCH;
  }
  else {
    double * d = (double *) (data + vtab.bfp_bytes);
    double threshold = *d;
    result = std::shared_ptr<RDtreeConstraint>(new RDtreeTanimoto(data, size, threshold));
  }

  return result;
}

RDtreeTanimoto::RDtreeTanimoto(const uint8_t * data, int size, double threshold_)
  : threshold(threshold_), bfp(data, data+size), bfp_filter(size, 0)
{
  weight = bfp_op_weight(size, data);
}

int RDtreeTanimoto::initialize(const RDtreeVtab & vtab)
{
  int rc = SQLITE_OK;

  std::fill(bfp_filter.begin(), bfp_filter.end(), 0);

  /* compute the required number of bit to be set */
  int na = weight;
  double t = threshold;
  int nbits = ceil((1 - t)*na) + 1;

#if 0
  /* Easy method first. Just pick the first nbits bits from aBfp, and 
  ** copy them onto aBfpFilter.
  */
  
  /* loop on the query fp and copy nbits bits 
  ** (we could do better using the bitfreq stats, but in a next rev)
  */
  u8 * pSrc = pCons->aBfp;
  u8 * pSrc_end = pSrc + iBfpSize;
  u8 * pDst = pCons->aBfpFilter;

  int i;
  int bitcount = 0;
  u8 bit, byte;

  while (bitcount < nbits && pSrc < pSrc_end) {
    byte = *pSrc++;
    for (i = 0, bit = 0x01; bitcount < nbits && i < 8; ++i, bit<<=1) {
      if (byte & bit) {
	++bitcount;
	*pDst |= bit;
      }
    }
    ++pDst;
  }

#else
  /* More sophisticated approach. Use the bit frequency table to pick
  ** the bits from bfp that are less frequently occurring in the database
  ** and may therefore provide a more selective power
  */

  char *zSql = sqlite3_mprintf(
    "SELECT bitno FROM '%q'.'%q_bitfreq' WHERE bitno IN (",
	vtab.db_name.c_str(), vtab.table_name.c_str());
  char *zTmp;

  bool first = true;
  for (int i=0; i < vtab.bfp_bytes; ++i) {
    uint8_t byte = bfp[i];
    uint8_t bit = 0x01;
    for (int ii = 0; ii < 8; ++ii, bit<<=1) {
      if (byte & bit) {
	    int bitno = i*8 + ii;
	    zTmp = zSql;
	    if (first) {
	      zSql = sqlite3_mprintf("%s%d", zTmp, bitno);
	      first = false;
        }
        else {
          zSql = sqlite3_mprintf("%s, %d", zTmp, bitno);
        }
	    sqlite3_free(zTmp);
      }
    }
  }
  
  if (zSql) {
    zTmp = zSql;
    zSql = sqlite3_mprintf("%s) ORDER BY freq ASC LIMIT %d;", zTmp, nbits);
    sqlite3_free(zTmp);
  }
  
  if (!zSql) {
    return SQLITE_NOMEM;
  }

  sqlite3_stmt * pStmt = 0;
  
  rc = sqlite3_prepare_v2(vtab.db, zSql, -1, &pStmt, 0);
  sqlite3_free(zSql);

  if (rc != SQLITE_OK) {
    return rc;
  }

  while ((rc = sqlite3_step(pStmt)) == SQLITE_ROW) {
    int bitno = sqlite3_column_int(pStmt, 0);
    bfp_filter[bitno/8] |= (0x01 << (bitno % 8));
  }

  sqlite3_finalize(pStmt);
  
  if (rc == SQLITE_DONE) {
    rc = SQLITE_OK;
  }
  
#endif
  
  assert(bfp_op_weight(bfp_filter.size(), bfp_filter.data()) == nbits);
  assert(bfp_op_contains(bfp.size(), bfp.data(), bfp_filter.data()));

  return rc;
}

int RDtreeTanimoto::test_internal(const RDtreeItem & item, bool & eof) const
{
  double t = threshold;
  int na = weight;
  
  /* It is known that for the tanimoto similarity to be above a given 
  ** threshold t, it must be
  **
  ** Na*t <= Nb <= Na/t
  **
  ** And for the fingerprints in pItem we have that 
  ** 
  ** iMinWeight <= Nb <= iMaxWeight
  **
  ** so if (Na*t > iMaxWeight) or (Na/t < iMinWeight) this item can be
  ** discarded.
  */
  if ((item.max_weight < t*na) || (na < t*item.min_weight)) {
    eof = true;
  }
  /*
  ** A bfp satisfying the query must have at least 1 bit in common with
  ** any subset of Na - t*Na + 1 bits from the query. If this test fails
  ** on the union of fingerprints populating the child nodes, we can prune 
  ** the subtree
  */
  else if (!bfp_op_intersects(item.bfp.size(), item.bfp.data(), bfp_filter.data())) {
    eof = true;
  }
  /* The item in the internal node stores the union of the fingerprints 
  ** that populate the child nodes. I want to use this union to compute an
  ** upper bound to the similarity. If this upper bound is lower than the 
  ** threashold value then the item can be discarded and the referred branch
  ** pruned.
  **
  ** T = Nsame / (Na + Nb - Nsame) <= Nsame / Na
  */
  else {
    int iweight = bfp_op_iweight(item.bfp.size(), item.bfp.data(), bfp.data());
    eof = (iweight < t*na);
  }
  return SQLITE_OK;
}

int RDtreeTanimoto::test_leaf(const RDtreeItem & item, bool & eof) const
{
  double t = threshold;
  int na = weight;
  int nb = item.max_weight; /* on a leaf node max == min*/
  
  if ((nb < t*na) || (na < t*nb)) {
    eof = true;
  }
  else if (!bfp_op_intersects(item.bfp.size(), item.bfp.data(), bfp_filter.data())) {
    eof = true;
  }
  else {
    int iweight = bfp_op_iweight(item.bfp.size(), item.bfp.data(), bfp.data());
    int uweight = na + nb - iweight;
    double similarity = uweight ? ((double)iweight)/uweight : 1.;
    
    eof = similarity < t;
  }
  return SQLITE_OK;
}

Blob RDtreeTanimoto::do_serialize() const
{
  Blob result(4 + bfp.size() + sizeof(double));
  uint8_t * p = result.data();
  p += write_uint32(p, RDTREE_TANIMOTO_CONSTRAINT_MAGIC);
  std::copy(bfp.begin(), bfp.end(), p);
  p += bfp.size();
  uint8_t * d = (uint8_t *) &threshold;
  std::copy(d, d+sizeof(double), p);
  return result;
}
