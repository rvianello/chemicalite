#ifndef CHEMICALITE_UTILITIES_INCLUDED
#define CHEMICALITE_UTILITIES_INCLUDED

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

#endif
