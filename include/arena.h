#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct ArenaChunk {
  struct ArenaChunk *next;
  size_t used;
  size_t cap;
  // flexible array member
  unsigned char data[];
} ArenaChunk;

typedef struct Arena {
  ArenaChunk *head;
  size_t chunk_size;
} Arena;

void arena_init(Arena *a, size_t chunk_size);
void arena_free(Arena *a);
void *arena_alloc(Arena *a, size_t size, size_t align);

#endif // ARENA_H

