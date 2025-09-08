#ifndef SQALE_H
#define SQALE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
  SQALE_MODE_REPL,
  SQALE_MODE_RUN,
  SQALE_MODE_EMIT_IR,
} SqaleMode;

typedef struct SqaleCtx SqaleCtx;

struct SqaleCtx {
  SqaleMode mode;
  const char *input_path;   // for run/emit-ir
  const char *output_path;  // for emit-ir
  bool use_llvm;            // runtime flag from compile-time define
};

int sqale_main(SqaleCtx *ctx, int argc, char **argv);

#endif // SQALE_H

