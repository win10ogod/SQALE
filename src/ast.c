#include "ast.h"
#include <stdlib.h>
#include <stdalign.h>
#include <string.h>

Node *node_new_list(Arena *arena, size_t cap_hint) {
  Node *n = (Node *)arena_alloc(arena, sizeof(Node), alignof(Node));
  n->kind = N_LIST; n->line=0; n->col=0; n->ty=NULL;
  size_t cap = cap_hint ? cap_hint : 4;
  n->as.list.items = (Node **)arena_alloc(arena, sizeof(Node*)*cap, alignof(Node*));
  n->as.list.count = 0;
  return n;
}

void node_list_push(Arena *arena, Node *list, Node *item) {
  size_t cnt = list->as.list.count;
  // simple grow by doubling: reallocate a new array and copy (arena append-only)
  // We can't realloc in arena: allocate new bigger, copy, update pointer.
  static const size_t base = 4;
  size_t cap = (cnt==0) ? base : cnt;
  while (cap <= cnt) cap *= 2;
  if (cnt==0 || (cnt & (cnt-1))==0) {
    Node **new_items = (Node **)arena_alloc(arena, sizeof(Node*)*cap, alignof(Node*));
    if (cnt>0) memcpy(new_items, list->as.list.items, sizeof(Node*)*cnt);
    list->as.list.items = new_items;
  }
  list->as.list.items[cnt] = item;
  list->as.list.count++;
}

Node *node_new_symbol(Arena *arena, const char *ptr, size_t len, size_t line, size_t col) {
  Node *n = (Node *)arena_alloc(arena, sizeof(Node), alignof(Node));
  n->kind=N_SYMBOL; n->line=line; n->col=col; n->ty=NULL;
  char *p = (char *)arena_alloc(arena, len+1, 1);
  memcpy(p, ptr, len); p[len]='\0';
  n->as.sym.ptr = p; n->as.sym.len=len;
  return n;
}

Node *node_new_int(Arena *arena, int64_t v, size_t line, size_t col) {
  Node *n = (Node *)arena_alloc(arena, sizeof(Node), alignof(Node));
  n->kind=N_INT; n->line=line; n->col=col; n->ty=NULL; n->as.ival=v; return n;
}
Node *node_new_float(Arena *arena, double v, size_t line, size_t col) {
  Node *n = (Node *)arena_alloc(arena, sizeof(Node), alignof(Node));
  n->kind=N_FLOAT; n->line=line; n->col=col; n->ty=NULL; n->as.fval=v; return n;
}
Node *node_new_string(Arena *arena, const char *ptr, size_t len, size_t line, size_t col) {
  Node *n = (Node *)arena_alloc(arena, sizeof(Node), alignof(Node));
  n->kind=N_STRING; n->line=line; n->col=col; n->ty=NULL;
  char *p = (char *)arena_alloc(arena, len+1, 1);
  memcpy(p, ptr, len); p[len]='\0';
  n->as.str.ptr=p; n->as.str.len=len; return n;
}
Node *node_new_bool(Arena *arena, bool v, size_t line, size_t col) {
  Node *n = (Node *)arena_alloc(arena, sizeof(Node), alignof(Node));
  n->kind=N_BOOL; n->line=line; n->col=col; n->ty=NULL; n->as.bval=v; return n;
}
