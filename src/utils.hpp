#ifndef CHEMICALITE_UTILITIES_INCLUDED
#define CHEMICALITE_UTILITIES_INCLUDED
#include <cstdint>
#include <vector>
#include <string>

using Blob = std::vector<std::uint8_t>;

#define XSTRINGIFY(s) STRINGIFY(s)
#define STRINGIFY(s) #s

template <void (*F)(sqlite3_context*, int, sqlite3_value**)>
static void strict(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
  for (int argn = 0; argn < argc; ++argn) {
    if (sqlite3_value_type(argv[argn]) == SQLITE_NULL) {
      /* if any argument is NULL, return NULL */
      sqlite3_result_null(ctx);
      return;
    }
  }
  /* otherwise call the wrapped function */
  F(ctx, argc, argv);
}

std::string trim(const std::string &);

inline uint16_t read_uint16(const uint8_t *p)
{
  return (p[0]<<8) + p[1];
}

inline uint32_t read_uint32(const uint8_t *p)
{
  return (
    (((uint32_t)p[0]) << 24) +
    (((uint32_t)p[1]) << 16) +
    (((uint32_t)p[2]) <<  8) +
    (((uint32_t)p[3]) <<  0)
  );
}

inline uint64_t read_uint64(const uint8_t *p)
{
  return (
    (((uint64_t)p[0]) << 56) +
    (((uint64_t)p[1]) << 48) +
    (((uint64_t)p[2]) << 40) +
    (((uint64_t)p[3]) << 32) +
    (((uint64_t)p[4]) << 24) +
    (((uint64_t)p[5]) << 16) +
    (((uint64_t)p[6]) <<  8) +
    (((uint64_t)p[7]) <<  0)
  );
}

inline int write_uint16(uint8_t *p, uint16_t i)
{
  p[0] = (i>> 8)&0xFF;
  p[1] = (i>> 0)&0xFF;
  return 2;
}

inline int write_uint32(uint8_t *p, uint32_t i)
{
  p[0] = (i>>24)&0xFF;
  p[1] = (i>>16)&0xFF;
  p[2] = (i>> 8)&0xFF;
  p[3] = (i>> 0)&0xFF;
  return 4;
}

inline int write_uint64(uint8_t *p, uint64_t i){
  p[0] = (i>>56)&0xFF;
  p[1] = (i>>48)&0xFF;
  p[2] = (i>>40)&0xFF;
  p[3] = (i>>32)&0xFF;
  p[4] = (i>>24)&0xFF;
  p[5] = (i>>16)&0xFF;
  p[6] = (i>> 8)&0xFF;
  p[7] = (i>> 0)&0xFF;
  return 8;
}

inline void sqlite3_result(sqlite3_context* ctx, int value) { sqlite3_result_int(ctx, value); }
inline void sqlite3_result(sqlite3_context* ctx, double value) { sqlite3_result_double(ctx, value); }
inline void sqlite3_result(sqlite3_context* ctx, const std::string & value)
{
  sqlite3_result_text(ctx, value.data(), value.size(), SQLITE_TRANSIENT);
}

#endif
