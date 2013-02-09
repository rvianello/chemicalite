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

int object_header_size();
int wrap_blob(u8 *pBlob, int sz, u32 type, Object **ppObject, int *pObjSz);
u8* get_blob(Object *pObject);

#endif
