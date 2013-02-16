#ifndef CHEMICALITE_BINARY_OBJECT_WRAPPER_INCLUDED
#define CHEMICALITE_BINARY_OBJECT_WRAPPER_INCLUDED

#define MOLOBJ  0x00000001
#define QMOLOBJ 0x00000002
#define BFPOBJ  0x00000004
 
typedef struct Object Object;

int object_header_size();
int is_object_type(Object * pObject, u32 type);

int wrap_blob(u8 *pBlob, int sz, u32 type, Object **ppObject, int *pObjSz);
u8* get_blob(Object *pObject);

#endif
