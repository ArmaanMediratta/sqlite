#include "vector.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

VectorStatus v_open(Vector** out)
{
  Vector* v = calloc(1, sizeof(Vector));
  if (v == NULL)
  {
    return VECTOR_ERR_NO_MEM;
  }

  if ((v->entries = calloc(1, sizeof(Entry*))) == NULL)
  {
    free(v);
    return VECTOR_ERR_NO_MEM;
  }

  v->capacity = 1;
  *out        = v;

  return VECTOR_OK;
}

VectorStatus v_close(Vector* v)
{
  if (v == NULL)
    return VECTOR_ERR_BAD_VECTOR;

  for (uint32_t i = 0; i < v->size; ++i)
  {
    free(v->entries[i]->data);
  }

  free(v->entries);
  free(v);
  return VECTOR_OK;
}

VectorStatus v_add(Vector* v, Entry* e)
{
  if (v == NULL)
    return VECTOR_ERR_BAD_VECTOR;

  if (v->capacity == v->size)
  {
    v->entries = realloc(v->entries, (v->capacity * 2) * (sizeof(Entry*)));
    if (v->entries == NULL)
      return VECTOR_ERR_NO_MEM;
    v->capacity *= 2;
  }

  v->entries[v->size] = e;
  ++v->size;

  return VECTOR_OK;
}
