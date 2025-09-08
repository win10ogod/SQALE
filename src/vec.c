#include "vec.h"
#include <stdlib.h>

void vec_init(Vec *v) { v->data = NULL; v->len = v->cap = 0; }

void vec_reserve(Vec *v, size_t cap) {
  if (v->cap >= cap) return;
  size_t nc = v->cap ? v->cap : 8;
  while (nc < cap) nc *= 2;
  void **nd = (void **)realloc(v->data, nc * sizeof(void*));
  if (!nd) return; // OOM ignored for simplicity
  v->data = nd; v->cap = nc;
}

void vec_push(Vec *v, void *item) {
  if (v->len + 1 > v->cap) vec_reserve(v, v->len + 1);
  v->data[v->len++] = item;
}

void *vec_get(Vec *v, size_t idx) { return (idx < v->len) ? v->data[idx] : NULL; }
void vec_set(Vec *v, size_t idx, void *item) { if (idx < v->len) v->data[idx] = item; }

void vec_free(Vec *v) { free(v->data); v->data = NULL; v->len = v->cap = 0; }

