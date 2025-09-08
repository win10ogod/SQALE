#ifndef RUNTIME_H
#define RUNTIME_H

#include <stddef.h>
#include <stdint.h>
#include "value.h"
#include "env.h"
#include "gc.h"
#include "arena.h"

typedef struct VM VM;

// VM holds global GC and root sets; definition is shared here
struct VM {
  struct GC gc;
  struct Env *global_env;
  struct ModNode *imported; // import cache
  struct ModArena *mod_arenas; // keep module arenas alive
};

typedef struct ModNode {
  char *path;
  struct ModNode *next;
} ModNode;

typedef struct ModArena {
  Arena *arena;
  struct ModArena *next;
} ModArena;

// String helpers
String *rt_string_new(VM *vm, const char *bytes, size_t len);
String *rt_string_from_cstr(VM *vm, const char *cstr);

// I/O (runtime stdlib abstractions)
Value rt_print(Env *env, Value *args, int nargs);
Value rt_read_file(Env *env, Value *args, int nargs);
Value rt_write_file(Env *env, Value *args, int nargs);
Value rt_str_split_ws(Env *env, Value *args, int nargs);

// Math
Value rt_add(Env *env, Value *args, int nargs);
Value rt_sub(Env *env, Value *args, int nargs);
Value rt_mul(Env *env, Value *args, int nargs);
Value rt_div(Env *env, Value *args, int nargs);
Value rt_eq(Env *env, Value *args, int nargs);
Value rt_lt(Env *env, Value *args, int nargs);
Value rt_gt(Env *env, Value *args, int nargs);

// Concurrency
typedef struct Channel Channel;
Value rt_chan(Env *env, Value *args, int nargs);
Value rt_send(Env *env, Value *args, int nargs);
Value rt_recv(Env *env, Value *args, int nargs);
Value rt_spawn(Env *env, Value *args, int nargs);

// Collections
Value rt_vec_new(Env *env, Value *args, int nargs);
Value rt_vec_push(Env *env, Value *args, int nargs);
Value rt_vec_get(Env *env, Value *args, int nargs);
Value rt_vec_len(Env *env, Value *args, int nargs);

// Code-as-data list/symbol helpers for macros
Value rt_is_list(Env *env, Value *args, int nargs);
Value rt_is_symbol(Env *env, Value *args, int nargs);
Value rt_symbol_eq(Env *env, Value *args, int nargs);
Value rt_list_len(Env *env, Value *args, int nargs);
Value rt_list_head(Env *env, Value *args, int nargs);
Value rt_list_tail(Env *env, Value *args, int nargs);
Value rt_list_cons(Env *env, Value *args, int nargs);
Value rt_list_append(Env *env, Value *args, int nargs);

// Maps (minimal: Str -> Int)
Value rt_map_new(Env *env, Value *args, int nargs);
Value rt_map_set(Env *env, Value *args, int nargs);
Value rt_map_get(Env *env, Value *args, int nargs);
Value rt_map_len(Env *env, Value *args, int nargs);

#endif // RUNTIME_H
