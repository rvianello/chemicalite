#include <cassert>
#include <iomanip>
#include <sstream>

#include <boost/algorithm/string.hpp>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include <GraphMol/ROMol.h>

#include "file_io.hpp"
#include "logging.hpp"


PropColumn * PropColumn::from_spec(const std::string & spec)
{
  // prepare a common error message prefix
  std::string error = "could not parse column specifier \"" + spec + "\": ";

  // where column-spec can be 
  //     prop-name type
  // or
  //     prop-name type as column-name
  // and prop-name / column-name can be enclosed in double quotes if needed.
  std::vector<std::string> tokens;

  std::istringstream specss(spec);
  int counter = 0;
  while (true) {
    std::string token;
    // the prop name and the column name may be in double quotes
    if (counter == 0 || counter == 3) {
      specss >> std::quoted(token);
    }
    else {
      specss >> token;
    }
    if (!specss) {
      break;
    }
    // normalize he constants as uppercase (when present), so that
    // they will be easier to verify/process
    if (counter == 1 || counter == 2) {
      boost::to_upper(token);
    }
    tokens.push_back(std::move(token));
    ++counter;
  }  

  int num_tokens = tokens.size();

  if (num_tokens == 2) {
    // Append two tokens and make "prop TYPE" equivalent to
    // "prop TYPE AS prop"
    tokens.push_back("AS");
    tokens.push_back(tokens[0]);
    num_tokens += 2;
  }

  if (num_tokens != 4) {
    error += "unexpected number of tokens";
    chemicalite_log(SQLITE_ERROR, error.c_str());
    return nullptr;
  }

  if (tokens[2] != "AS") {
    error += "third token should be \"AS\"";
    chemicalite_log(SQLITE_ERROR, error.c_str());
    return nullptr;
  }

  const std::string & property = tokens[0];
  const std::string & type_term = tokens[1];
  const std::string & column = tokens[3];
  Type type = Type::TEXT;
  if (type_term == "REAL") {
    type = Type::REAL;
  }
  else if (type_term == "INTEGER") {
    type = Type::INTEGER;
  }
  else if (type_term != "TEXT") {
    error += "type should be one of \"TEXT\", \"REAL\" or \"INTEGER\"";
    chemicalite_log(SQLITE_ERROR, error.c_str());
    return nullptr;
  }

  return new PropColumn{type, property, column};
}

const char * PropColumn::sql_type() const
{
  switch (type) {
    case Type::TEXT: return "TEXT";
    case Type::REAL: return "REAL";
    case Type::INTEGER: return "INTEGER";
    default:
      assert(!"Unexpected value for Type enum");
      return "";
  }
}

std::string PropColumn::declare_column() const
{
  return "\"" + column + "\" " + sql_type();
}

void PropColumn::sqlite3_result(const RDKit::ROMol & mol, sqlite3_context * ctx) const
{
  if (!mol.hasProp(property)) {
    sqlite3_result_null(ctx);
    return;
  }
  try {
    switch (type) {
      case Type::TEXT:
        sqlite3_result_text(ctx, mol.getProp<std::string>(property).c_str(), -1, SQLITE_TRANSIENT);
        break;
      case Type::REAL:
        sqlite3_result_double(ctx, mol.getProp<double>(property));
        break;
      case Type::INTEGER:
        sqlite3_result_int(ctx, mol.getProp<int>(property));
        break;
      default:
        assert(!"Unexpected value for Type enum");
    }
  }
  catch (const boost::bad_any_cast & e) {
    chemicalite_log(SQLITE_MISMATCH, "could not convert the mol property to the requested type");
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
  }
}


int parse_schema(const std::string & schema, PropColumnPtrs & columns)
{
  // make sure the arg value wasn't just blank spaces
  if (schema.empty()) {
    std::string error = "could not parse schema: arg value is blank";
    chemicalite_log(SQLITE_ERROR, error.c_str());
    return SQLITE_ERROR;
  }

  // split on commas, and parse the columns specs
  std::size_t spec_pos = 0;
  std::size_t sep_pos = schema.find(',');
  while (spec_pos < schema.size()) {
    // fetch the substring covering the column spec
    std::size_t spec_len = (sep_pos == std::string::npos) ? sep_pos : sep_pos - spec_pos;
    std::string spec(schema, spec_pos, spec_len);
    boost::trim(spec);
    // process and store the column spec
    PropColumnPtr column(PropColumn::from_spec(spec));
    if (column) {
      columns.push_back(std::move(column));
    }
    else {
      std::string error = "could not configure a column from \"" + spec + "\"";
      chemicalite_log(SQLITE_ERROR, error.c_str());
      return SQLITE_ERROR;
    }
    // move spec pos after the separator (or to the end)
    spec_pos = (sep_pos == std::string::npos) ? sep_pos : sep_pos + 1;
    // find the next separator
    sep_pos = schema.find(',', spec_pos);
  }

  return SQLITE_OK;
}
