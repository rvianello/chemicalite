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

#define UNUSED(x) do { (void)(x); } while (0)

#endif