#ifndef GC_H
#define GC_H

#include <stddef.h>
#include <stdbool.h>

typedef struct Obj {
  struct Obj *next;
  unsigned marked : 1;
  unsigned type : 7; // for debugging
} Obj;

typedef struct GC {
  Obj *objects;
  size_t bytes_allocated;
  size_t next_threshold;
  void (*mark_root_cb)(void *user);
  void *user;
} GC;

void gc_init(GC *gc);
void gc_set_root_callback(GC *gc, void (*cb)(void *), void *user);
void *gc_alloc(GC *gc, size_t sz, unsigned type_tag);
void gc_collect(GC *gc);
void gc_mark(Obj *o);
void gc_free_all(GC *gc);

#endif // GC_H

