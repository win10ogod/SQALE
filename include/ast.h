#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "arena.h"

typedef struct Type Type; // forward

typedef enum {
  N_LIST,
  N_SYMBOL,
  N_INT,
  N_FLOAT,
  N_STRING,
  N_BOOL,
} NodeKind;

typedef struct Node Node;

struct Node {
  NodeKind kind;
  size_t line, col;
  Type *ty; // inferred/checked type, set by type checker
  union {
    struct { Node **items; size_t count; } list;
    struct { const char *ptr; size_t len; } sym;
    int64_t ival;
    double fval;
    struct { const char *ptr; size_t len; } str;
    bool bval;
  } as;
};

typedef struct Parser Parser;

// Construction helpers
Node *node_new_list(Arena *arena, size_t cap_hint);
void node_list_push(Arena *arena, Node *list, Node *item);
Node *node_new_symbol(Arena *arena, const char *ptr, size_t len, size_t line, size_t col);
Node *node_new_int(Arena *arena, int64_t v, size_t line, size_t col);
Node *node_new_float(Arena *arena, double v, size_t line, size_t col);
Node *node_new_string(Arena *arena, const char *ptr, size_t len, size_t line, size_t col);
Node *node_new_bool(Arena *arena, bool v, size_t line, size_t col);

#endif // AST_H

