#include <cassert>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <memory>

#include <boost/algorithm/string.hpp>

#include <GraphMol/FileParsers/MolSupplier.h>
#include <GraphMol/FileParsers/MolWriters.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"
#include "sdf_io.hpp"
#include "mol.hpp"
#include "logging.hpp"

struct SdfColumn {
  enum class Type { TEXT, REAL, INTEGER };

  Type type;
  std::string property;
  std::string column;

  static SdfColumn * from_spec(const std::string & spec)
  {
    // prepare a common error message prefix
    std::string error = "could not parse column specifier \"" + spec + "\": ";

    // where column-spec can be 
    //     prop-name type
    // or
    //     prop-name type as column-name
    // and prop-name / column-name can be enclosed in single quotes if needed.
    std::vector<std::string> tokens;

    std::istringstream specss(spec);
    int counter = 0;
    while (true) {
      std::string token;
      // the prop name and the column name may be in single quotes
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

    return new SdfColumn{type, property, column};
  }

  const char * sql_type() const
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

  std::string declare_column() const
  {
    return "\"" + column + "\" " + sql_type();
  }

  void sqlite3_result(const RDKit::ROMol & mol, sqlite3_context * ctx) const
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

};


class SdfReaderVtab : public sqlite3_vtab {
public:
  std::string filename;
  std::vector<std::unique_ptr<SdfColumn>> columns;
  bool is_function;

  SdfReaderVtab()
  {
    nRef = 0;
    pModule = 0;
    zErrMsg = 0;
  }

  int init(sqlite3 *db, int argc, const char * const *argv, char **pzErr)
  {
    // used like a table-valued function
    if ((argc == 3) && (std::string(argv[0]) == std::string(argv[2]))) {
      is_function = true;

      int rc = sqlite3_declare_vtab(db, "CREATE TABLE x(molecule MOL, filename TEXT HIDDEN)");

      if (rc != SQLITE_OK) {
        *pzErr = sqlite3_mprintf("%s", sqlite3_errmsg(db));
      }

      return rc;
    }

    if (argc < 4) {
      chemicalite_log(
        SQLITE_ERROR, "the sdf_reader virtual table requires at least one filename argument");
      return SQLITE_ERROR;
    }

    // used like a regular table
    is_function = false;
    std::istringstream filename_ss(argv[3]);
    filename_ss >> std::quoted(filename, '\'');

    for (int idx = 4; idx < argc; ++idx) {
      std::string arg = argv[idx];
      // optional args are expected to be formatted as 
      // name = value
      // where value let's say could be of a numeric type or a quoted string
      const std::size_t eq_pos = arg.find('=');
      if (eq_pos == std::string::npos) {
        // unable to parse optional arg, '=' not found
        std::string error = "could not parse \"" + arg + "\": optional arg expression should include an equal sign";
        chemicalite_log(SQLITE_ERROR, error.c_str());
        return SQLITE_ERROR;
      }
      // we want to fetch the substring after the equal sign, make sure that
      // it's not the last char in the string.
      if (eq_pos == arg.size() - 1) {
        std::string error = "could not parse \"" + arg + "\": no arg value following the equal sign";
        chemicalite_log(SQLITE_ERROR, error.c_str());
        return SQLITE_ERROR;
      }
      std::string arg_name(arg, 0, eq_pos);
      boost::trim(arg_name);
      // because at this time 'schema' is the only supported optional arg,
      // we keep things simple
      if (arg_name != "schema") {
        std::string error = "could not parse \"" + arg + "\": unexpected arg name: " + arg_name;
        chemicalite_log(SQLITE_ERROR, error.c_str());
        return SQLITE_ERROR;
      }
      // we expect the value to be in quotes, and consist in a comma-separated
      // list of mol properties, that need to be exposed as table columns
      // "column-spec, column-spec, ...,column-spec"
      std::string arg_value;
      std::istringstream arg_value_ss(std::string(arg, eq_pos+1, std::string::npos));
      arg_value_ss >> std::quoted(arg_value, '\'');
      // make sure the arg value wasn't just blank spaces
      if (arg_value.empty()) {
        std::string error = "could not parse \"" + arg + "\": arg value is blank";
        chemicalite_log(SQLITE_ERROR, error.c_str());
        return SQLITE_ERROR;
      }
      // split on commas, and parse the columns specs
      std::size_t spec_pos = 0;
      std::size_t sep_pos = arg_value.find(',');
      while (spec_pos < arg_value.size()) {
        // fetch the substring covering the column spec
        std::size_t spec_len = (sep_pos == std::string::npos) ? sep_pos : sep_pos - spec_pos;
        std::string spec(arg_value, spec_pos, spec_len);
        boost::trim(spec);
        // process and store the column spec
        std::unique_ptr<SdfColumn> column(SdfColumn::from_spec(spec));
        if (column) {
          columns.push_back(std::move(column));
        }
        else {
          std::string error = "could not configure a column from \"" + arg + "\"";
          chemicalite_log(SQLITE_ERROR, error.c_str());
          return SQLITE_ERROR;
        }
        // move spec pos after the separator (or to the end)
        spec_pos = (sep_pos == std::string::npos) ? sep_pos : sep_pos + 1;
        // find the next separator
        sep_pos = arg_value.find(',', spec_pos);
      }
    }

    std::string sql_declaration = "CREATE TABLE x(molecule MOL";
    for (const auto & column: columns) {
      sql_declaration += ", " + column->declare_column();
    }
    sql_declaration += ")";

    int rc = sqlite3_declare_vtab(db, sql_declaration.c_str());

    if (rc != SQLITE_OK) {
      *pzErr = sqlite3_mprintf("%s", sqlite3_errmsg(db));
    }

    return rc;
  }

};

static int sdfReaderInit(sqlite3 *db, void */*pAux*/,
                      int argc, const char * const *argv,
                      sqlite3_vtab **ppVTab,
                      char **pzErr)
{  
  SdfReaderVtab *vtab = new SdfReaderVtab;

  int rc = vtab->init(db, argc, argv, pzErr);

  if (rc == SQLITE_OK) {
    *ppVTab = (sqlite3_vtab *)vtab;
  }
  else {
    delete vtab;
  }

  return rc;
}

int sdfReaderBestIndex(sqlite3_vtab *pVTab, sqlite3_index_info *pIndexInfo)
{
  SdfReaderVtab *vtab = (SdfReaderVtab *)pVTab;

  /* Two different cases exist, depending on whether the module is used as a plain
  ** table, or as a table-valued function.
  */

  if (vtab->is_function) {
    // We expect the filename to be provided as an argument to the table-valued function
    // and made available to this code as an equality constraint on the hidden filename
    // column (the 2nd column in the declared vtab schema)
    bool found = false;
    for (int ii=0; ii<pIndexInfo->nConstraint; ++ii) {
      if (pIndexInfo->aConstraint[ii].iColumn == 1 &&
          pIndexInfo->aConstraint[ii].op == SQLITE_INDEX_CONSTRAINT_EQ) {
        if (!pIndexInfo->aConstraint[ii].usable) {
          return SQLITE_CONSTRAINT;
        }
        pIndexInfo->aConstraintUsage[ii].argvIndex = 1;
        found = true;
        break;
      }
    }
    if (!found) {
      chemicalite_log(
        SQLITE_ERROR, "the sdf_reader function requires a filename argument");
      return SQLITE_ERROR;
    }
  }

  /* A forward scan is the only supported mode, this method is therefore very minimal */
  pIndexInfo->estimatedCost = 100000; 
  return SQLITE_OK;
}

static int sdfReaderDisconnectDestroy(sqlite3_vtab *pVTab)
{
  SdfReaderVtab *vtab = (SdfReaderVtab *)pVTab;
  delete vtab;
  return SQLITE_OK;
}

struct SdfReaderCursor : public sqlite3_vtab_cursor {
  std::string filename;
  std::unique_ptr<RDKit::ForwardSDMolSupplier> supplier;
  sqlite3_int64 rowid;
  std::unique_ptr<RDKit::ROMol> mol;
};

static int sdfReaderOpen(sqlite3_vtab */*pVTab*/, sqlite3_vtab_cursor **ppCursor)
{
  int rc = SQLITE_OK;
  SdfReaderCursor *pCsr = new SdfReaderCursor; 
  *ppCursor = (sqlite3_vtab_cursor *)pCsr;
  return rc;
}

static int sdfReaderClose(sqlite3_vtab_cursor *pCursor)
{
  SdfReaderCursor *p = (SdfReaderCursor *)pCursor;
  delete p;
  return SQLITE_OK;
}

static int sdfReaderFilter(sqlite3_vtab_cursor *pCursor, int /*idxNum*/, const char */*idxStr*/,
                     int argc, sqlite3_value **argv)
{
  SdfReaderCursor *p = (SdfReaderCursor *)pCursor;
  SdfReaderVtab *vtab = (SdfReaderVtab *)p->pVtab;

  if (vtab->is_function) {
    // a single arg is expected
    if (argc != 1) {
      chemicalite_log(
        SQLITE_ERROR, "the sdf_reader function expects one single argument");
      return SQLITE_ERROR;
    }

    // of string/text type
    sqlite3_value *arg = argv[0];
    int value_type = sqlite3_value_type(arg);
    if (value_type != SQLITE_TEXT) {
      chemicalite_log(
        SQLITE_ERROR, "the sdf_reader function requires a filename argument of type TEXT");
      return SQLITE_MISMATCH;
    }

    p->filename = (const char *)sqlite3_value_text(arg);
  }
  else {
    p->filename = vtab->filename;
  }

  std::unique_ptr<std::ifstream> pins(new std::ifstream(p->filename));

  if (!pins->is_open()) {
    // Maybe log something
    std::string message = "could not open file '" + p->filename + "'";
    chemicalite_log(SQLITE_ERROR, message.c_str());
    return SQLITE_ERROR;
  }

  p->supplier.reset(new RDKit::ForwardSDMolSupplier(pins.release(), true));
  if (!p->supplier->atEnd()) {
    p->rowid = 1;
    p->mol.reset(p->supplier->next());
  }

  return SQLITE_OK;
}

static int sdfReaderNext(sqlite3_vtab_cursor *pCursor)
{
  SdfReaderCursor * p = (SdfReaderCursor *)pCursor;
  if (!p->supplier->atEnd()) {
    p->rowid += 1;
    p->mol.reset(p->supplier->next());
  }
  return SQLITE_OK;
}

static int sdfReaderEof(sqlite3_vtab_cursor *pCursor)
{
  SdfReaderCursor * p = (SdfReaderCursor *)pCursor;
  return p->supplier->atEnd() ? 1 : 0;
}

static int sdfReaderColumn(sqlite3_vtab_cursor *pCursor, sqlite3_context *ctx, int N)
{
  SdfReaderCursor * p = (SdfReaderCursor *)pCursor;

  if (!p->mol) {
      sqlite3_result_null(ctx);
  }
  else if (N == 0) {
    // the molecule
    if (p->mol) {
      int rc = SQLITE_OK;
      Blob blob = mol_to_blob(*p->mol, &rc);
      if (rc == SQLITE_OK) {
        sqlite3_result_blob(ctx, blob.data(), blob.size(), SQLITE_TRANSIENT);
      }
      else {
        sqlite3_result_error_code(ctx, rc);
      }
    }
    else {
      sqlite3_result_null(ctx);
    }
  }
  else {
    SdfReaderVtab * vtab = (SdfReaderVtab *) p->pVtab;
    
    if (vtab->is_function) {
      sqlite3_result_text(ctx, p->filename.c_str(), -1, SQLITE_TRANSIENT);
    }
    else {
      // The requested index must map to one of the additional columns
      // that provide access to the mol properties
      assert(N <= int(vtab->columns.size()));
      vtab->columns[N-1]->sqlite3_result(*p->mol, ctx);
    }
  }

  return SQLITE_OK;
}

static int sdfReaderRowid(sqlite3_vtab_cursor *pCursor, sqlite_int64 *pRowid)
{
  SdfReaderCursor * p = (SdfReaderCursor *)pCursor;
  *pRowid = p->rowid;
  return SQLITE_OK;
}

/*
** The SDF reader module
*/
static sqlite3_module sdfReaderModule = {
  0,                           /* iVersion */
  sdfReaderInit,               /* xCreate - create a table */
  sdfReaderInit,               /* xConnect - connect to an existing table */
  sdfReaderBestIndex,          /* xBestIndex - Determine search strategy */
  sdfReaderDisconnectDestroy,  /* xDisconnect - Disconnect from a table */
  sdfReaderDisconnectDestroy,  /* xDestroy - Drop a table */
  sdfReaderOpen,               /* xOpen - open a cursor */
  sdfReaderClose,              /* xClose - close a cursor */
  sdfReaderFilter,             /* xFilter - configure scan constraints */
  sdfReaderNext,               /* xNext - advance a cursor */
  sdfReaderEof,                /* xEof */
  sdfReaderColumn,             /* xColumn - read data */
  sdfReaderRowid,              /* xRowid - read data */
  0,                           /* xUpdate - write data */
  0,                           /* xBegin - begin transaction */
  0,                           /* xSync - sync transaction */
  0,                           /* xCommit - commit transaction */
  0,                           /* xRollback - rollback transaction */
  0,                           /* xFindFunction - function overloading */
  0,                           /* xRename - rename the table */
  0,                           /* xSavepoint */
  0,                           /* xRelease */
  0,                           /* xRollbackTo */
  0                            /* xShadowName */
};


struct SdfWriterAggregateContext {
  std::shared_ptr<RDKit::SDWriter> writer;
};

void sdf_writer_step(sqlite3_context* ctx, int, sqlite3_value** argv)
{
  // get the input args
  sqlite3_value *filename_arg = argv[0];

  if (sqlite3_value_type(filename_arg) == SQLITE_NULL) {
    chemicalite_log(SQLITE_MISUSE, "filename argument is not allowed to be null");
    sqlite3_result_error_code(ctx, SQLITE_MISUSE);
    return;
  }

  if (sqlite3_value_type(filename_arg) != SQLITE_TEXT) {
    chemicalite_log(SQLITE_MISMATCH, "filename argument must be text");
    sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
    return;
  }

  std::string filename((const char *)sqlite3_value_text(filename_arg));

  sqlite3_value *mol_arg = argv[1];

  std::unique_ptr<RDKit::ROMol> mol;
  if (sqlite3_value_type(mol_arg) != SQLITE_NULL) {
    int rc = SQLITE_OK;
    mol.reset(arg_to_romol(mol_arg, &rc));
    if (rc != SQLITE_OK) {
      chemicalite_log(SQLITE_MISMATCH, "invalid molecule input");
      sqlite3_result_error_code(ctx, rc);
      return;
    }
  } 

  // get the aggregate context
  void **agg = (void **) sqlite3_aggregate_context(ctx, sizeof(void *));

  if (!agg) {
    // this should be *very* unlikely
    chemicalite_log(SQLITE_NOMEM, "sd_writer_step no aggregate context");
    sqlite3_result_error_code(ctx, SQLITE_NOMEM);
    return;
  }  

  if (!*agg) {
    // open the output file stream, allocate an aggregate context, and initialize the writer
    std::unique_ptr<std::ofstream> pouts(new std::ofstream(filename));

    if (!pouts->is_open()) {
      std::string message = "could not open file '" + filename + "'";
      chemicalite_log(SQLITE_ERROR, message.c_str());
      sqlite3_result_error(ctx, message.c_str(), -1);
      return;
    }

    // create the SD writer, instructing it to take ownership of the ofstream pointer
    std::shared_ptr<RDKit::SDWriter> writer(new RDKit::SDWriter(pouts.release(), true));
    // create the aggregate context, and assign the writer to it
    std::unique_ptr<SdfWriterAggregateContext> context(new SdfWriterAggregateContext);
    context->writer = writer;

    *agg = (void *) context.release();
  }

  SdfWriterAggregateContext * context = (SdfWriterAggregateContext *) *agg;

  if (mol) {
    context->writer->write(*mol); // can this throw?
    context->writer->flush();
  }
}

void sdf_writer_final(sqlite3_context * ctx)
{
  // get the aggregate context
  void **agg = (void **) sqlite3_aggregate_context(ctx, 0);

  if (!agg) {
    sqlite3_result_null(ctx);
    return;
  }

  if (*agg) {
    SdfWriterAggregateContext * context = (SdfWriterAggregateContext *) *agg;

    context->writer->close();

    int num_mols = context->writer->numMols();
    if (num_mols > 0) {
      sqlite3_result_int(ctx, num_mols);
    }
    else {
      sqlite3_result_null(ctx);
    }
  }
}


int chemicalite_init_sdf_io(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) {
    rc = sqlite3_create_module_v2(db, "sdf_reader", &sdfReaderModule, 
				  0,  /* Client data for xCreate/xConnect */
				  0   /* Module destructor function */
				  );
  }

  rc = sqlite3_create_window_function(
    db,
    "sdf_writer",
    2, // int nArg,
    SQLITE_UTF8, // int eTextRep,
    0, // void *pApp,
    sdf_writer_step, // void (*xStep)(sqlite3_context*,int,sqlite3_value**),
    sdf_writer_final, // void (*xFinal)(sqlite3_context*),
    0, // void (*xValue)(sqlite3_context*),
    0, // void (*xInverse)(sqlite3_context*,int,sqlite3_value**),
    0  // void(*xDestroy)(void*)
  );

  return rc;
}
