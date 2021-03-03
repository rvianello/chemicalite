#include <cstring>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"
#include "bfp.hpp"
#include "logging.hpp"

static constexpr const uint32_t BFP_MAGIC = 0x42465000;

Blob bfp_to_blob(const std::string & bfp, int *)
{
  Blob blob(sizeof(uint32_t) + bfp.size());
  uint8_t * p = blob.data();
  p += write_uint32(p, BFP_MAGIC);
  memcpy(p, bfp.data(), bfp.size());
  return blob;
}

std::string blob_to_bfp(const Blob & blob, int *rc)
{
  if (blob.size() <= sizeof(uint32_t)) {
    *rc = SQLITE_MISMATCH;
    return "";
  }
  const uint8_t * p = blob.data();
  uint32_t magic = read_uint32(p);
  if (magic != BFP_MAGIC) {
    *rc = SQLITE_MISMATCH;
    return "";
  }
  p += sizeof(uint32_t);
  return std::string(reinterpret_cast<const char *>(p), blob.size()-sizeof(uint32_t));
}

std::string arg_to_bfp(sqlite3_value *arg, int *rc)
{
  int value_type = sqlite3_value_type(arg);

  *rc = SQLITE_OK;

  if (value_type != SQLITE_BLOB) {
    *rc = SQLITE_MISMATCH;
    chemicalite_log(SQLITE_MISMATCH, "input arg must be of type blob or NULL");
    return "";
  }

  int size = sqlite3_value_bytes(arg);
  const uint8_t * data = (const uint8_t *)sqlite3_value_blob(arg);
  Blob blob(data, data+size);
  return blob_to_bfp(blob, rc);
}
