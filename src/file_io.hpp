#ifndef CHEMICALITE_PROP_IO_INCLUDED
#define CHEMICALITE_PROP_IO_INCLUDED
#include <string>
#include <vector>
#include <memory>

namespace RDKit
{
  class ROMol;
} // namespace RDKit

struct PropColumn {
  enum class Type { TEXT, REAL, INTEGER };

  Type type;
  std::string property;
  std::string column;

  static PropColumn * from_spec(const std::string & spec);
  const char * sql_type() const;
  std::string declare_column() const;
  void sqlite3_result(const RDKit::ROMol & mol, sqlite3_context * ctx) const;
};

using PropColumnPtr = std::unique_ptr<PropColumn>;
using PropColumnPtrs = std::vector<PropColumnPtr>;

int parse_schema(const std::string & schema, PropColumnPtrs &);

#endif
