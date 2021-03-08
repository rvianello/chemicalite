#include <utility>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"
#include "bfp_descriptors.hpp"
#include "bfp.hpp"
#include "bfp_ops.hpp"


template <typename F, F f>
static void bfp_descriptor(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv)
{
  sqlite3_value *arg = argv[0];

  int rc = SQLITE_OK;
  std::string bfp = arg_to_bfp(arg, &rc);

  if ( rc != SQLITE_OK ) {
    sqlite3_result_error_code(ctx, rc);
  }
  else {
    typename std::result_of<F(const std::string &)>::type descriptor = f(bfp);
    sqlite3_result(ctx, descriptor);
  }
}

static int bfp_length(const std::string & bfp) {return 8*bfp.size();}

static int bfp_weight(const std::string & bfp)
{
  return bfp_op_weight(bfp.size(), reinterpret_cast<const uint8_t *>(bfp.data()));
}

#define BFP_DESCRIPTOR(func) bfp_descriptor<decltype(&func), &func>

int chemicalite_init_bfp_descriptors(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "bfp_length", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<BFP_DESCRIPTOR(bfp_length)>, 0, 0);
  if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "bfp_weight", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, strict<BFP_DESCRIPTOR(bfp_weight)>, 0, 0);
 
  return rc;
}
