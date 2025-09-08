#include "gc.h"
#include <stdlib.h>

void gc_init(GC *gc) {
  gc->objects = NULL;
  gc->bytes_allocated = 0;
  gc->next_threshold = 1024*1024; // 1MB default
  gc->mark_root_cb = NULL;
  gc->user = NULL;
}

void gc_set_root_callback(GC *gc, void (*cb)(void *), void *user) {
  gc->mark_root_cb = cb; gc->user = user;
}

void *gc_alloc(GC *gc, size_t sz, unsigned type_tag) {
  Obj *o = (Obj*)malloc(sz);
  o->marked = 0; o->type = type_tag;
  o->next = gc->objects; gc->objects = o;
  gc->bytes_allocated += sz;
  if (gc->bytes_allocated > gc->next_threshold) {
    gc_collect(gc);
    gc->next_threshold *= 2;
  }
  return o;
}

void gc_mark(Obj *o) {
  if (!o || o->marked) return;
  o->marked = 1;
  // Minimal GC: Objects are leaf nodes for now (Strings, Channels, Closures not traversed here)
}

static void sweep(GC *gc) {
  Obj **cur = &gc->objects;
  while (*cur) {
    if (!(*cur)->marked) {
      Obj *unreached = *cur;
      *cur = unreached->next;
      free(unreached);
    } else {
      (*cur)->marked = 0; // unmark for next cycle
      cur = &(*cur)->next;
    }
  }
}

void gc_collect(GC *gc) {
  if (gc->mark_root_cb) gc->mark_root_cb(gc->user);
  sweep(gc);
}

void gc_free_all(GC *gc) {
  Obj *o = gc->objects;
  while (o) { Obj *n = o->next; free(o); o = n; }
  gc->objects = NULL; gc->bytes_allocated = 0; gc->next_threshold = 1024*1024;
}

