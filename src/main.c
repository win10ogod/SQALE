#include "sqale.h"
#include "parser.h"
#include "eval.h"
#include "codegen.h"
#include "arena.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file_all(const char *path, size_t *out_len) {
  FILE *f = fopen(path, "rb"); if (!f) return NULL;
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  char *buf = (char*)malloc(n+1); if (!buf){ fclose(f); return NULL; }
  size_t r = fread(buf,1,n,f); fclose(f); buf[r]='\0'; if(out_len)*out_len=r; return buf;
}

static int cmd_repl(void) {
  VM *vm = vm_new();
  Arena arena; arena_init(&arena, 1<<20); // persistent arena for REPL state
  char line[4096];
  printf("SQALE REPL. Ctrl-D to exit.\n");
  while (1) {
    printf("> "); fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) break;
    Parser p; parser_init(&p, &arena, line, strlen(line));
    Node *n_raw = parse_toplevel(&p);
    MacroEnv *menv = NULL; macros_register_core(&menv);
    VM *mvm = vm_new(); macros_collect_user(&arena, &menv, mvm, n_raw);
    Node *n = macro_expand_all(&arena, menv, n_raw);
    for (size_t i=0;i<n->as.list.count;i++) {
      Value out; if (eval_form(vm, n->as.list.items[i], &out)==0) {
        // print result unless it's Unit
        if (out.kind==VAL_UNIT) continue;
        switch (out.kind) {
          case VAL_INT: printf("%lld\n", (long long)out.as.i); break;
          case VAL_FLOAT: printf("%g\n", out.as.f); break;
          case VAL_BOOL: printf(out.as.b?"true\n":"false\n"); break;
          case VAL_STR: printf("\"%.*s\"\n", (int)out.as.str->len, out.as.str->data); break;
          default: printf("<val>\n"); break;
        }
      }
    }
  }
  arena_free(&arena);
  vm_free(vm);
  return 0;
}

static int cmd_run(const char *path) {
  size_t n=0; char *buf = read_file_all(path, &n);
  if (!buf) { fprintf(stderr, "failed to read %s\n", path); return 1; }
  Arena arena; arena_init(&arena, 1<<20);
  Parser p; parser_init(&p, &arena, buf, n);
  Node *prog_raw = parse_toplevel(&p);
  MacroEnv *menv = NULL; macros_register_core(&menv);
  VM *mvm = vm_new(); macros_collect_user(&arena, &menv, mvm, prog_raw);
  Node *prog = macro_expand_all(&arena, menv, prog_raw);
  VM *vm = vm_new();
  int rc = eval_program(vm, prog);
  if (rc==0) {
    EnvEntry *e = env_lookup(vm->global_env, "main");
    if (e && e->value) {
      Value v = *(Value*)e->value;
      if (v.kind==VAL_CLOSURE) {
        Value r = vm_call_closure0(vm, v.as.clos);
        if (r.kind==VAL_INT) rc = (int)r.as.i;
      }
    }
  }
  vm_free(vm); vm_free(mvm); arena_free(&arena); free(buf); return rc;
}

static int cmd_emit_ir(const char *path, const char *out_path) {
  size_t n=0; char *buf = read_file_all(path, &n);
  if (!buf) { fprintf(stderr, "failed to read %s\n", path); return 1; }
  Arena arena; arena_init(&arena, 1<<20);
  Parser p; parser_init(&p, &arena, buf, n);
  Node *prog_raw = parse_toplevel(&p);
  MacroEnv *menv = NULL; macros_register_core(&menv);
  VM *mvm = vm_new(); macros_collect_user(&arena, &menv, mvm, prog_raw);
  Node *prog = macro_expand_all(&arena, menv, prog_raw);

  // Run type checking to populate type annotations on AST nodes
  VM *vm = vm_new();
  int rc = eval_program(vm, prog);
  if (rc != 0) {
    fprintf(stderr, "Type checking failed\n");
    vm_free(vm); vm_free(mvm); arena_free(&arena); free(buf);
    return 1;
  }

  CodegenOpts opts = { .module_name = path, .use_llvm = USE_LLVM, .for_exe = 1 };
  size_t out_len=0; char *ir = codegen_emit_ir(prog, &opts, &out_len);
  FILE *f = fopen(out_path?out_path:"out.ll", "wb"); if (!f) { free(buf); arena_free(&arena); free(ir); vm_free(vm); return 1; }
  fwrite(ir,1,out_len,f); fclose(f);
  free(ir); vm_free(vm); vm_free(mvm); arena_free(&arena); free(buf); return 0;
}

static int cmd_build(const char *path, const char *out_path) {
  // Emits IR; user can compile with clang if available
  int rc = cmd_emit_ir(path, out_path?out_path:"out.ll");
  if (rc==0) {
    fprintf(stdout, "IR emitted to %s. Compile with: clang %s -O2 -o a.out\n", out_path?out_path:"out.ll", out_path?out_path:"out.ll");
  }
  return rc;
}

int main(int argc, char **argv) {
  if (argc<2) {
    fprintf(stderr, "Usage: %s [repl | run <file.sq> | emit-ir <file.sq> -o <out.ll>]\n", argv[0]);
    return 1;
  }
  if (strcmp(argv[1], "repl")==0) return cmd_repl();
  if (strcmp(argv[1], "run")==0 && argc>=3) return cmd_run(argv[2]);
  if (strcmp(argv[1], "emit-ir")==0 && argc>=3) {
    const char *out="out.ll";
    for (int i=3;i<argc-1;i++) if (strcmp(argv[i], "-o")==0) out=argv[i+1];
    return cmd_emit_ir(argv[2], out);
  }
  if (strcmp(argv[1], "build")==0 && argc>=3) {
    const char *out="out.ll";
    for (int i=3;i<argc-1;i++) if (strcmp(argv[i], "-o")==0) out=argv[i+1];
    return cmd_build(argv[2], out);
  }
  fprintf(stderr, "Invalid command.\n");
  return 1;
}
