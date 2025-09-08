#ifndef CODEGEN_H
#define CODEGEN_H

#include <stddef.h>
#include "ast.h"

typedef struct CodegenOpts {
  const char *module_name;
  int use_llvm; // 1 if compiled with LLVM
  int for_exe;  // emit main entrypoint
} CodegenOpts;

// Emit textual LLVM IR for the program into an in-memory buffer.
// Returns a malloc'd buffer that the caller must free; size is stored in out_len.
char *codegen_emit_ir(Node *program, const CodegenOpts *opts, size_t *out_len);

#endif // CODEGEN_H
