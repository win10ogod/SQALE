#ifndef EVAL_H
#define EVAL_H

#include "value.h"
#include "ast.h"
#include "env.h"
#include "runtime.h"
#include "gc.h"

typedef struct VM VM; // in runtime.h too

VM *vm_new(void);
void vm_free(VM *vm);

// Evaluate a fully parsed and typechecked program at top-level
// Returns 0 on success, non-zero on error.
int eval_program(VM *vm, Node *program);

// Evaluate a single form (used by REPL)
int eval_form(VM *vm, Node *form, Value *out);

// Call a closure value with argv/argc; returns result in Value
Value vm_call_closure(VM *vm, struct Closure *c, Value *args, int nargs);
Value vm_call_closure0(VM *vm, struct Closure *c);

// Import and evaluate a SQALE source file into the VM
int vm_import_file(VM *vm, const char *path);

#endif // EVAL_H
