#ifndef VEC_H
#define VEC_H

#include <stddef.h>

typedef struct {
  void **data;
  size_t len;
  size_t cap;
} Vec;

void vec_init(Vec *v);
void vec_reserve(Vec *v, size_t cap);
void vec_push(Vec *v, void *item);
void *vec_get(Vec *v, size_t idx);
void vec_set(Vec *v, size_t idx, void *item);
void vec_free(Vec *v);

#endif // VEC_H

