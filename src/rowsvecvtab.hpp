#ifndef CHEMICALITE_ROWS_VEC_VTAB_INCLUDED
#define CHEMICALITE_ROWS_VEC_VTAB_INCLUDED

template <typename T>
int rowsVecOpen(sqlite3_vtab */*pVTab*/, sqlite3_vtab_cursor **ppCursor)
{
  int rc = SQLITE_OK;
  T *pCsr = new T;
  *ppCursor = (sqlite3_vtab_cursor *)pCsr;
  return rc;
}

template <typename T>
int rowsVecClose(sqlite3_vtab_cursor *pCursor)
{
  T *p = (T *)pCursor;
  delete p;
  return SQLITE_OK;
}

template <typename T>
int rowsVecNext(sqlite3_vtab_cursor *pCursor)
{
  T * p = (T *)pCursor;
  p->index += 1;
  return SQLITE_OK;
}

template <typename T>
int rowsVecEof(sqlite3_vtab_cursor *pCursor)
{
  T * p = (T *)pCursor;
  return p->index >= p->rows.size() ? 1 : 0;
}

template <typename T>
int rowsVecRowid(sqlite3_vtab_cursor *pCursor, sqlite_int64 *pRowid)
{
  T * p = (T *)pCursor;
  *pRowid = p->index + 1;
  return SQLITE_OK;
}

template <typename T>
struct RowsVecCursor : public sqlite3_vtab_cursor {
  uint32_t index;
  std::vector<T> rows;
};

#endif
