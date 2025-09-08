#ifndef STR_H
#define STR_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} Str;

void str_init(Str *s);
void str_reserve(Str *s, size_t need);
void str_append(Str *s, const char *cstr);
void str_append_n(Str *s, const char *buf, size_t n);
void str_pushc(Str *s, char c);
void str_clear(Str *s);
void str_free(Str *s);

#endif // STR_H

