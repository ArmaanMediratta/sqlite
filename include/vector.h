#ifndef VECTOR_H
#define VECTOR_H

#include <stdint.h>

typedef enum
{
  VECTOR_OK,
  VECTOR_ERR_NO_MEM,
  VECOR_ERR_BAD_VECTOR,
} VectorStatus;

typedef struct
{
  void* data;
} Entry;

typedef struct
{
  Entry** entires;
  uint32_t capacity;
  uint32_t size;
} Vector;

VectorStatus v_open(Vector** out);
VectorStatus v_close(Vector* v);

VectorStatus v_add(Vector* v, Entry* e);

#endif