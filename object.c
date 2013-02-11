#include <assert.h>
#include <string.h>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "chemicalite.h"
#include "object.h"

#define MAGIC_MASK 0xFFFFFF00
#define TYPE_MASK  0x000000FF
#define MAGIC      0xABCDEF00

#define OBJMAGIC(p) (*((u32 *)p) & MAGIC_MASK)
#define OBJTYPE(p) (*((u32 *)p) & TYPE_MASK)
#define IS_OBJPTR(p) (OBJMAGIC(p) == MAGIC)

struct Object {
  u32 marker;
  u8 blob[];
};

int object_header_size() 
{ 
  return sizeof(Object); 
}

int is_object_type(Object * pObject, u32 type)
{
  return IS_OBJPTR(pObject) && (OBJTYPE(pObject) | type);
}

u8* get_blob(Object *pObject) 
{ 
  assert(pObject);
  return pObject->blob; 
}

/* 
** wraps the binary blob pointed by pBlob in an Object structure where
** it's prefixed by a data type marker
*/
int wrap_blob(u8 *pBlob, int sz, u32 type, Object **ppObject, int *pObjSz)
{
  assert(pBlob);
  assert(sz);

  int rc = SQLITE_NOMEM;
  int objsz = sizeof(Object) + sz;
  *ppObject = sqlite3_malloc(objsz);
  if (*ppObject) {
    *pObjSz = objsz;
    (*ppObject)->marker = MAGIC | type;
    memcpy((*ppObject)->blob, pBlob, sz);
    rc = SQLITE_OK;
  }
  return rc;
}
