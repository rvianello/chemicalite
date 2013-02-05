#ifndef CHEMICALITE_BINARY_OBJECT_WRAPPER_INCLUDED
#define CHEMICALITE_BINARY_OBJECT_WRAPPER_INCLUDED

#define MAGIC_MASK 0xFFFFFF00
#define TYPE_MASK  0x000000FF
#define MAGIC      0xABCDEF00

#define MOLOBJ  0x00000001
#define QMOLOBJ 0x00000002

#define OBJMAGIC(p) (*((u32 *)p) & MAGIC_MASK)
#define OBJTYPE(p) (*((u32 *)p) & TYPE_MASK)
#define IS_OBJPTR(p) (OBJMAGIC(p) == MAGIC)
#define IS_MOLOBJ(p) (IS_OBJPTR(p) && (OBJTYPE(p) == MOLOBJ))
#define IS_QMOLOBJ(p) (IS_OBJPTR(p) && (OBJTYPE(p) == QMOLOBJ))

typedef struct Object Object;

struct Object {
  u32 marker;
  u8 blob[];
};

/* 
** wraps the binary blob pointed by pBlob in an Object structure where
** it's prefixed by a data type marker
*/
static int wrap_object(u8 *pBlob, int sz, u32 type, 
		       Object **ppObject, int *pObjSz)
{
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

#else
#error "object module included multiple times"
#endif
