#include <cctype>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"

std::string trim(const std::string & orig)
{
  std::size_t from = 0;
  std::size_t to = orig.size();

  while (from < to && isspace(orig[from])) {
    ++from;
  }

  if (from == to) {
    return "";
  }

  while (isspace(orig[to-1])) {
    --to;
  }

  return std::string(orig, from, to - from);
}
