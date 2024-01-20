#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <GraphMol/FileParsers/MolSupplier.h>
#include <GraphMol/FileParsers/MolWriters.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "utils.hpp"
#include "file_io.hpp"
#include "smi_io.hpp"
#include "mol.hpp"
#include "logging.hpp"


class SmiReaderVtab : public sqlite3_vtab {
public:
  std::string filename;
  std::string delimiter;
  int smiles_column;
  int name_column;
  bool title_line;
  std::vector<std::unique_ptr<PropColumn>> columns;
  bool is_function;

  SmiReaderVtab()
    : filename(), delimiter(" \t"), smiles_column(0), name_column(1), title_line(true)
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

      int rc = sqlite3_declare_vtab(
        db, 
        "CREATE TABLE x(molecule MOL,"
        " filename TEXT HIDDEN,"
        " delimiter TEXT HIDDEN,"
        " smiles_column INTEGER HIDDEN,"
        " name_column INTEGER HIDDEN,"
        " title_line BOOL HIDDEN"
        ")");

      if (rc != SQLITE_OK) {
        *pzErr = sqlite3_mprintf("%s", sqlite3_errmsg(db));
      }

      return rc;
    }

    if (argc < 4) {
      chemicalite_log(
        SQLITE_ERROR, "the smi_reader virtual table requires at least one filename argument");
      return SQLITE_ERROR;
    }

    // used like a regular table
    is_function = false;
    std::istringstream filename_ss(argv[3]);
    filename_ss >> std::quoted(filename, '\'');

    if (argc > 9) {
      chemicalite_log(
        SQLITE_ERROR, "the sdf_reader virtual table expects at most five optional arguments (delimiter, smiles_column, name_column, title_line, schema)");
      return SQLITE_ERROR;
    }
  
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
      std::string arg_value(arg, eq_pos+1, std::string::npos);
      boost::trim(arg_value);

      if (arg_name == "delimiter") {
        std::istringstream arg_value_ss(arg_value);
        arg_value_ss >> std::quoted(delimiter, '\'');
        if (delimiter.empty()) {
          std::string error = "could not parse \"" + arg + "\": invalid delimiter value";
          chemicalite_log(SQLITE_ERROR, error.c_str());
          return SQLITE_ERROR;
        }
      }
      else if (arg_name == "smiles_column") {
        try {
          size_t pos;
          smiles_column = std::stoi(arg_value, &pos);
        }
        catch (...) {
          std::string error = "could not parse \"" + arg + "\": invalid integer value";
          chemicalite_log(SQLITE_ERROR, error.c_str());
          return SQLITE_ERROR;
        }
      }
      else if (arg_name == "name_column") {
        try {
          size_t pos;
          name_column = std::stoi(arg_value, &pos);
        }
        catch (...) {
          std::string error = "could not parse \"" + arg + "\": invalid integer value";
          chemicalite_log(SQLITE_ERROR, error.c_str());
          return SQLITE_ERROR;
        }
      }
      else if (arg_name == "title_line") {
        try {
          title_line = boost::lexical_cast<bool>(arg_value);
        }
        catch (...) {
          std::string error = "could not parse \"" + arg + "\": invalid value for a bool arg";
          chemicalite_log(SQLITE_ERROR, error.c_str());
          return SQLITE_ERROR;
        }
      }
      else if (arg_name == "schema") {
        // we expect the schema spec string to be in quotes, and consist in a comma-separated
        // list of mol properties, that need to be exposed as table columns
        // "column-spec, column-spec, ...,column-spec"
        std::istringstream arg_value_ss(arg_value);
        arg_value_ss >> std::quoted(arg_value, '\'');
        int rc = parse_schema(arg_value, columns);
        if (rc != SQLITE_OK) {
          return rc;
        }
      } else {
        std::string error = "could not parse \"" + arg + "\": unexpected arg name: " + arg_name;
        chemicalite_log(SQLITE_ERROR, error.c_str());
        return SQLITE_ERROR;
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

static int smiReaderInit(sqlite3 *db, void */*pAux*/,
                      int argc, const char * const *argv,
                      sqlite3_vtab **ppVTab,
                      char **pzErr)
{  
  SmiReaderVtab *vtab = new SmiReaderVtab;

  int rc = vtab->init(db, argc, argv, pzErr);

  if (rc == SQLITE_OK) {
    *ppVTab = (sqlite3_vtab *)vtab;
  }
  else {
    delete vtab;
  }

  return rc;
}

enum SmiReaderColumn : int {
  MOLECULE = 0,
  FILENAME = 1,
  DELIMITER = 2,
  SMILES_COLUMN = 3,
  NAME_COLUMN = 4,
  TITLE_LINE = 5
};

int smiReaderBestIndex(sqlite3_vtab *pVTab, sqlite3_index_info *pIndexInfo)
{
  SmiReaderVtab *vtab = (SmiReaderVtab *)pVTab;

  /* Two different cases exist, depending on whether the module is used as a plain
  ** table, or as a table-valued function.
  */

  if (vtab->is_function) {
    // We expect the filename to be provided as an argument to the table-valued function
    // and made available to this code as an equality constraint on the hidden filename
    // column (the 2nd column in the declared vtab schema).
    // The additional arguments (delimiter, smiles and name columns, title line availability)
    // are optional.
    int queryplan_mask = 0;
    int col_pos[6];
    col_pos[0] = col_pos[1] = col_pos[2] = col_pos[3] = col_pos[4] = col_pos[5] = -1;

    for (int ii=0; ii<pIndexInfo->nConstraint; ++ii) {
      if (pIndexInfo->aConstraint[ii].iColumn < SmiReaderColumn::FILENAME) {
        // only consider the constraints corresponding to the mol supplier arguments
        continue;
      }
      if (!pIndexInfo->aConstraint[ii].usable) {
        // this is a argument we need, report this plan as not usable  
        return SQLITE_CONSTRAINT;
      }
      int col = pIndexInfo->aConstraint[ii].iColumn;
      int mask = 1 << col;
      queryplan_mask |= mask;
      col_pos[col] = ii;
    }

    if (col_pos[SmiReaderColumn::FILENAME] < 0) {
      chemicalite_log(
        SQLITE_ERROR, "the smi_reader function requires a filename argument");
      return SQLITE_ERROR;
    }

    // pass the available args to xFilter in their column order
    int argc = 0;
    for (int col=SmiReaderColumn::FILENAME; col <= SmiReaderColumn::TITLE_LINE; ++col) {
      int pos = col_pos[col];
      if (pos >= 0) {
        pIndexInfo->aConstraintUsage[pos].argvIndex = ++argc;
        pIndexInfo->aConstraintUsage[pos].omit = true;
      }
    }

    // use idxNum to inform xFilter about which args are available
    pIndexInfo->idxNum = queryplan_mask;
  }

  pIndexInfo->estimatedCost = 100000; 
  return SQLITE_OK;
}

static int smiReaderDisconnectDestroy(sqlite3_vtab *pVTab)
{
  SmiReaderVtab *vtab = (SmiReaderVtab *)pVTab;
  delete vtab;
  return SQLITE_OK;
}

struct SmiReaderCursor : public sqlite3_vtab_cursor {
  std::string filename;
  std::string delimiter;
  int smiles_column;
  int name_column;
  bool title_line;
  std::unique_ptr<RDKit::SmilesMolSupplier> supplier;
  bool eof;
  sqlite3_int64 rowid;
  std::unique_ptr<RDKit::ROMol> mol;

  SmiReaderCursor()
    : filename(), delimiter(" \t"), smiles_column(0), name_column(1), title_line(true) {};

  int next();
};

int SmiReaderCursor::next()
{
  try {
    rowid += 1;
    mol.reset(supplier->next());
  }
  catch (...) {
    if (supplier->atEnd()) {
      eof = true;
    }
    else {
      std::string message = "error reading file '" + filename + "'";
      chemicalite_log(SQLITE_ERROR, message.c_str());
      return SQLITE_ERROR;
    }
  }
  return SQLITE_OK;
}

static int smiReaderOpen(sqlite3_vtab */*pVTab*/, sqlite3_vtab_cursor **ppCursor)
{
  int rc = SQLITE_OK;
  SmiReaderCursor *pCsr = new SmiReaderCursor; 
  *ppCursor = (sqlite3_vtab_cursor *)pCsr;
  return rc;
}

static int smiReaderClose(sqlite3_vtab_cursor *pCursor)
{
  SmiReaderCursor *p = (SmiReaderCursor *)pCursor;
  delete p;
  return SQLITE_OK;
}

static int smiReaderFilter(sqlite3_vtab_cursor *pCursor, int idxNum, const char */*idxStr*/,
                     int argc, sqlite3_value **argv)
{
  SmiReaderCursor *p = (SmiReaderCursor *)pCursor;
  SmiReaderVtab *vtab = (SmiReaderVtab *)p->pVtab;

  if (vtab->is_function) {
    // when this virtual table is used as a function, the mol supplier args are to be found in
    // the available query constraints.

    int query_mask = idxNum;

    // at least one arg is expected (filename)
    if ((argc < 1) || !(query_mask & (1 << SmiReaderColumn::FILENAME))) {
      chemicalite_log(
        SQLITE_ERROR, "the smi_reader function expects at least one filename argument");
      return SQLITE_ERROR;
    }

    // of string/text type
    sqlite3_value *arg = argv[0];
    int value_type = sqlite3_value_type(arg);
    if (value_type != SQLITE_TEXT) {
      chemicalite_log(
        SQLITE_ERROR, "the smi_reader function requires a filename argument of type TEXT");
      return SQLITE_MISMATCH;
    }
    p->filename = (const char *)sqlite3_value_text(arg);

    int argn = 1;

    if ((argn < argc) && (query_mask & (1 << SmiReaderColumn::DELIMITER))) {
      sqlite3_value *arg = argv[argn++];
      int value_type = sqlite3_value_type(arg);
      if (value_type != SQLITE_TEXT) {
        chemicalite_log(
          SQLITE_ERROR, "the smi_reader function expects the delimiter argument to be of type TEXT");
        return SQLITE_MISMATCH;
      }
      p->delimiter = (const char *)sqlite3_value_text(arg);
    }

    if ((argn < argc) && (query_mask & (1 << SmiReaderColumn::SMILES_COLUMN))) {
      sqlite3_value *arg = argv[argn++];
      int value_type = sqlite3_value_type(arg);
      if (value_type != SQLITE_INTEGER) {
        chemicalite_log(
          SQLITE_ERROR, "the smi_reader function expects the smiles_column argument to be of type INTEGER");
        return SQLITE_MISMATCH;
      }
      p->smiles_column = sqlite3_value_int(arg);
    }

    if ((argn < argc) && (query_mask & (1 << SmiReaderColumn::NAME_COLUMN))) {
      sqlite3_value *arg = argv[argn++];
      int value_type = sqlite3_value_type(arg);
      if (value_type != SQLITE_INTEGER) {
        chemicalite_log(
          SQLITE_ERROR, "the smi_reader function expects the name_column argument to be of type INTEGER");
        return SQLITE_MISMATCH;
      }
      p->name_column = sqlite3_value_int(arg);
    }

    if ((argn < argc) && (query_mask & (1 << SmiReaderColumn::TITLE_LINE))) {
      sqlite3_value *arg = argv[argn++];
      int value_type = sqlite3_value_type(arg);
      if (value_type != SQLITE_INTEGER) {
        chemicalite_log(
          SQLITE_ERROR, "the smi_reader function expects the title_line argument to be of type INTEGER (bool)");
        return SQLITE_MISMATCH;
      }
      p->title_line = sqlite3_value_int(arg);
    }

  }
  else {
    // get the mol supplier args from the virtual table instance
    p->filename = vtab->filename;
    p->delimiter = vtab->delimiter;
    p->smiles_column = vtab->smiles_column;
    p->name_column = vtab->name_column;
    p->title_line = vtab->title_line;
  }

  std::unique_ptr<std::ifstream> pins(new std::ifstream(p->filename));

  if (!pins->is_open()) {
    // Maybe log something
    std::string message = "could not open file '" + p->filename + "'";
    chemicalite_log(SQLITE_ERROR, message.c_str());
    return SQLITE_ERROR;
  }

  p->supplier.reset(new RDKit::SmilesMolSupplier(
    pins.release(), true, p->delimiter,
    p->smiles_column, p->name_column, p->title_line));
  p->rowid = 0;
  p->eof = false;

  return p->next();
}

static int smiReaderNext(sqlite3_vtab_cursor *pCursor)
{
  SmiReaderCursor * p = (SmiReaderCursor *)pCursor;
  return p->next();
}

static int smiReaderEof(sqlite3_vtab_cursor *pCursor)
{
  SmiReaderCursor * p = (SmiReaderCursor *)pCursor;
  return p->eof ? 1 : 0;
}

static int smiReaderColumn(sqlite3_vtab_cursor *pCursor, sqlite3_context *ctx, int N)
{
  SmiReaderCursor * p = (SmiReaderCursor *)pCursor;

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
    SmiReaderVtab * vtab = (SmiReaderVtab *) p->pVtab;
    
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

static int smiReaderRowid(sqlite3_vtab_cursor *pCursor, sqlite_int64 *pRowid)
{
  SmiReaderCursor * p = (SmiReaderCursor *)pCursor;
  *pRowid = p->rowid;
  return SQLITE_OK;
}


/*
** The smiles file reader module
*/
static sqlite3_module smiReaderModule = {
#if SQLITE_VERSION_NUMBER >= 3044000
  4,                           /* iVersion */
#else
  3,                           /* iVersion */
#endif
  smiReaderInit,               /* xCreate - create a table */
  smiReaderInit,               /* xConnect - connect to an existing table */
  smiReaderBestIndex,          /* xBestIndex - Determine search strategy */
  smiReaderDisconnectDestroy,  /* xDisconnect - Disconnect from a table */
  smiReaderDisconnectDestroy,  /* xDestroy - Drop a table */
  smiReaderOpen,               /* xOpen - open a cursor */
  smiReaderClose,              /* xClose - close a cursor */
  smiReaderFilter,             /* xFilter - configure scan constraints */
  smiReaderNext,               /* xNext - advance a cursor */
  smiReaderEof,                /* xEof */
  smiReaderColumn,             /* xColumn - read data */
  smiReaderRowid,              /* xRowid - read data */
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
#if SQLITE_VERSION_NUMBER >= 3044000
  ,
  0                            /* xIntegrity */
#endif
};


struct SmiWriterAggregateContext {
  std::shared_ptr<RDKit::SmilesWriter> writer;
};

void smi_writer_step(sqlite3_context* ctx, int argc, sqlite3_value**argv)
{
  // get the input args
  sqlite3_value *mol_arg = argv[0];

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

  sqlite3_value *filename_arg = argv[1];

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

  // assign the default values to the optional args
  std::string delimiter = " ";
  std::string name_header = "Name";
  bool include_header = true;
  bool isomeric_smiles = true;

  if (argc > 2) {
    sqlite3_value *arg = argv[2];
    int type = sqlite3_value_type(arg);
    if (type == SQLITE_NULL) {
      ; // ok, keep the default value
    }
    else if (type != SQLITE_TEXT) {
      chemicalite_log(SQLITE_MISMATCH, "delimiter argument must be text");
      sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
      return;
    }
    else {
      delimiter = (const char *)sqlite3_value_text(arg);
    }
  }
  
  if (argc > 3) {
    sqlite3_value *arg = argv[3];
    int type = sqlite3_value_type(arg);
    if (type == SQLITE_NULL) {
      ; // ok, keep the default value
    }
    else if (type != SQLITE_TEXT) {
      chemicalite_log(SQLITE_MISMATCH, "name_header argument must be text");
      sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
      return;
    }
    else {
      name_header = (const char *)sqlite3_value_text(arg);
    }
  }

  if (argc > 4) {
    sqlite3_value *arg = argv[4];
    int type = sqlite3_value_type(arg);
    if (type == SQLITE_NULL) {
      ; // ok, keep the default value
    }
    else if (type != SQLITE_INTEGER) {
      chemicalite_log(SQLITE_MISMATCH, "include_header argument must be integer (bool)");
      sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
      return;
    }
    else {
      include_header = (bool)sqlite3_value_int(arg);
    }
  }
  
  if (argc > 5) {
    sqlite3_value *arg = argv[5];
    int type = sqlite3_value_type(arg);
    if (type == SQLITE_NULL) {
      ; // ok, keep the default value
    }
    else if (type != SQLITE_INTEGER) {
      chemicalite_log(SQLITE_MISMATCH, "isomeric_smiles argument must be integer (bool)");
      sqlite3_result_error_code(ctx, SQLITE_MISMATCH);
      return;
    }
    else {
      isomeric_smiles = (bool)sqlite3_value_int(arg);
    }
  }
  
  // get the aggregate context
  void **agg = (void **) sqlite3_aggregate_context(ctx, sizeof(void *));

  if (!agg) {
    // this should be *very* unlikely
    chemicalite_log(SQLITE_NOMEM, "smi_writer_step no aggregate context");
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

    // create the smiles writer, instructing it to take ownership of the ofstream pointer
    bool take_ownership = true;
    std::shared_ptr<RDKit::SmilesWriter> writer(new RDKit::SmilesWriter(
        pouts.release(), delimiter, name_header, include_header, take_ownership, isomeric_smiles));

    // create the aggregate context, and assign the writer to it
    std::unique_ptr<SmiWriterAggregateContext> context(new SmiWriterAggregateContext);
    context->writer = writer;

    *agg = (void *) context.release();
  }

  SmiWriterAggregateContext * context = (SmiWriterAggregateContext *) *agg;

  if (mol) {
    context->writer->write(*mol); // can this throw?
    context->writer->flush();
  }
}

void smi_writer_final(sqlite3_context * ctx)
{
  // get the aggregate context
  void **agg = (void **) sqlite3_aggregate_context(ctx, 0);

  if (!agg) {
    sqlite3_result_null(ctx);
    return;
  }

  if (*agg) {
    SmiWriterAggregateContext * context = (SmiWriterAggregateContext *) *agg;

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


int chemicalite_init_smi_io(sqlite3 *db)
{
  int rc = SQLITE_OK;

  if (rc == SQLITE_OK) {
    rc = sqlite3_create_module_v2(db, "smi_reader", &smiReaderModule, 
				  0,  /* Client data for xCreate/xConnect */
				  0   /* Module destructor function */
				  );
  }

  for (int nargs=2; nargs<7; ++nargs) {
    rc = sqlite3_create_window_function(
      db,
      "smi_writer",
      nargs,
      SQLITE_UTF8, // int eTextRep,
      0, // void *pApp,
      smi_writer_step, // void (*xStep)(sqlite3_context*,int,sqlite3_value**),
      smi_writer_final, // void (*xFinal)(sqlite3_context*),
      0, // void (*xValue)(sqlite3_context*),
      0, // void (*xInverse)(sqlite3_context*,int,sqlite3_value**),
      0  // void(*xDestroy)(void*)
    );
  }

  return rc;
}
