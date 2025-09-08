#include "arena.h"
#include <stdlib.h>
#include <string.h>

static ArenaChunk *arena_chunk_new(size_t size) {
  ArenaChunk *c = (ArenaChunk *)malloc(sizeof(ArenaChunk) + size);
  if (!c) return NULL;
  c->next = NULL;
  c->used = 0;
  c->cap = size;
  return c;
}

void arena_init(Arena *a, size_t chunk_size) {
  a->chunk_size = (chunk_size < 4096) ? 4096 : chunk_size;
  a->head = arena_chunk_new(a->chunk_size);
}

void arena_free(Arena *a) {
  ArenaChunk *c = a->head;
  while (c) {
    ArenaChunk *n = c->next;
    free(c);
    c = n;
  }
  a->head = NULL;
}

void *arena_alloc(Arena *a, size_t size, size_t align) {
  if (align == 0) align = sizeof(void*);
  ArenaChunk *c = a->head;
  if (!c) {
    c = arena_chunk_new(a->chunk_size);
    a->head = c;
  }
  size_t off = (c->used + (align - 1)) & ~(align - 1);
  if (off + size > c->cap) {
    size_t need = size + align + sizeof(ArenaChunk);
    size_t newcap = a->chunk_size;
    while (newcap < need) newcap *= 2;
    ArenaChunk *n = arena_chunk_new(newcap);
    n->next = c;
    a->head = c = n;
    off = (c->used + (align - 1)) & ~(align - 1);
  }
  void *p = c->data + off;
  c->used = off + size;
  return p;
}

