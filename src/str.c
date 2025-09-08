#include "str.h"
#include <stdlib.h>
#include <string.h>

static void str_grow(Str *s, size_t mincap) {
  size_t cap = s->cap ? s->cap : 32;
  while (cap < mincap) cap *= 2;
  char *nd = (char *)realloc(s->data, cap);
  if (!nd) return; // OOM: leave as-is; caller should check
  s->data = nd;
  s->cap = cap;
}

void str_init(Str *s) {
  s->data = NULL; s->len = 0; s->cap = 0;
}

void str_reserve(Str *s, size_t need) {
  if (s->cap < need) str_grow(s, need);
}

void str_append(Str *s, const char *cstr) {
  size_t n = strlen(cstr);
  if (s->len + n + 1 > s->cap) str_grow(s, s->len + n + 1);
  memcpy(s->data + s->len, cstr, n);
  s->len += n;
  s->data[s->len] = '\0';
}

void str_append_n(Str *s, const char *buf, size_t n) {
  if (s->len + n + 1 > s->cap) str_grow(s, s->len + n + 1);
  memcpy(s->data + s->len, buf, n);
  s->len += n;
  s->data[s->len] = '\0';
}

void str_pushc(Str *s, char c) {
  if (s->len + 2 > s->cap) str_grow(s, s->len + 2);
  s->data[s->len++] = c;
  s->data[s->len] = '\0';
}

void str_clear(Str *s) {
  s->len = 0; if (s->data) s->data[0] = '\0';
}

void str_free(Str *s) {
  free(s->data); s->data = NULL; s->len = s->cap = 0;
}

