#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"
#include "bfp_compare.hpp"
#include "bfp.hpp"
#include "bfp_ops.hpp"

template <double (*F)(const std::string &, const std::string &)>
static void bfp_compare(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  double similarity = 0.;
  int rc = SQLITE_OK;

  std::string *p1 = 0;
  std::string *p2 = 0;

  void * aux1 = sqlite3_get_auxdata(ctx, 0);
  if (aux1) {
    p1 = (std::string *) aux1;
  }
  else {
    std::string bfp = arg_to_bfp(argv[0], &rc);
    if (rc != SQLITE_OK) {
      sqlite3_result_error_code(ctx, rc);
      return;
    }
    else {
      p1 = new std::string(std::move(bfp));
      sqlite3_set_auxdata(ctx, 0, (void *) p1, free_bfp_auxdata);
    }
  }

  void * aux2 = sqlite3_get_auxdata(ctx, 1);
  if (aux2) {
    p2 = (std::string *) aux2;
  }
  else {
    std::string bfp = arg_to_bfp(argv[1], &rc);
    if (rc != SQLITE_OK) {
      sqlite3_result_error_code(ctx, rc);
      return;
    }
    else {
      p2 = new std::string(std::move(bfp));
      sqlite3_set_auxdata(ctx, 1, (void *) p2, free_bfp_auxdata);
    }
  }

  if (!p1 || !p2) {
    sqlite3_result_null(ctx);
  }
  else if (p1->size() != p2->size()) {
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
  }
  else {
    similarity = F(*p1, *p2);
    sqlite3_result_double(ctx, similarity);
  }
}

static double tanimoto_similarity(const std::string & bfp1, const std::string & bfp2)
{
  return bfp_op_tanimoto(
    bfp1.size(),
    reinterpret_cast<const uint8_t *>(bfp1.data()),
    reinterpret_cast<const uint8_t *>(bfp2.data()));
}

static double dice_similarity(const std::string & bfp1, const std::string & bfp2)
{
  return bfp_op_dice(
    bfp1.size(),
    reinterpret_cast<const uint8_t *>(bfp1.data()),
    reinterpret_cast<const uint8_t *>(bfp2.data()));
}

int chemicalite_init_bfp_compare(sqlite3 *db)
{
  int rc = SQLITE_OK;
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "bfp_tanimoto", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<bfp_compare<tanimoto_similarity>>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "bfp_dice", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<bfp_compare<dice_similarity>>, 0, 0);
  return rc;
}

