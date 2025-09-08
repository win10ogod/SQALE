#ifndef MACRO_H
#define MACRO_H

#include "ast.h"
#include "arena.h"

typedef struct MacroEnv MacroEnv;
typedef Node *(*MacroFn)(Arena *a, Node *form);

typedef struct MacroClosure {
  struct VM *vm;     // macro-time VM
  struct Closure *c; // user closure
} MacroClosure;

struct MacroEnv {
  const char *name;
  int is_closure; // 0=c-fn, 1=closure
  MacroFn fn;
  MacroClosure clos;
  MacroEnv *next;
};

void macro_env_add(MacroEnv **env, const char *name, MacroFn fn);
void macro_env_add_closure(MacroEnv **env, const char *name, struct VM *vm, struct Closure *c);
Node *macro_expand_all(Arena *a, MacroEnv *env, Node *n);

// Built-in macro installers
void macros_register_core(MacroEnv **env);

// User macros: collect [defmacro ...] from a program into MacroEnv
void macros_collect_user(Arena *a, MacroEnv **env, struct VM *vm, Node *program);

#endif // MACRO_H
