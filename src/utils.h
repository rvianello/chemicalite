#ifndef CHEMICALITE_UTILITIES_INCLUDED
#define CHEMICALITE_UTILITIES_INCLUDED

#define CREATE_SQLITE_FUNCTION(a, func, rc)				\
  if (rc == SQLITE_OK)							\
    rc = sqlite3_create_function(db, # func, a, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0, func##_f, 0, 0)

#define CREATE_SQLITE_UNARY_FUNCTION(func, rc)	\
  CREATE_SQLITE_FUNCTION(1, func, rc)
#define CREATE_SQLITE_BINARY_FUNCTION(func, rc) \
  CREATE_SQLITE_FUNCTION(2, func, rc)

#endif
