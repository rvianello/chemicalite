#ifndef CHEMICALITE_UTILITIES_INCLUDED
#define CHEMICALITE_UTILITIES_INCLUDED

#define XSTRINGIFY(s) STRINGIFY(s)
#define STRINGIFY(s) #s

#define CREATE_SQLITE_FUNCTION(a, func)				\
  if (rc == SQLITE_OK)							\
    rc = sqlite3_create_function(db, STRINGIFY(func), a, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, func##_f, 0, 0)

#define CREATE_SQLITE_NULLARY_FUNCTION(func) CREATE_SQLITE_FUNCTION(0, func)
#define CREATE_SQLITE_UNARY_FUNCTION(func) CREATE_SQLITE_FUNCTION(1, func)
#define CREATE_SQLITE_BINARY_FUNCTION(func) CREATE_SQLITE_FUNCTION(2, func)
#define CREATE_SQLITE_TERNARY_FUNCTION(func) CREATE_SQLITE_FUNCTION(3, func)

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

#endif
