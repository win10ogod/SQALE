#include "codegen.h"
#include "str.h"
#include "ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if USE_LLVM
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/BitWriter.h>
#endif

static void append(Str *s, const char *t){ str_append(s, t); }
static void append_quoted(Str *s, const char *t){ str_pushc(s, '"'); str_append(s, t); str_pushc(s, '"'); }

static int find_main_fn(Node *program) {
  for (size_t i=0;i<program->as.list.count;i++) {
    Node *f = program->as.list.items[i];
    if (f->kind!=N_LIST || f->as.list.count<5) continue;
    if (f->as.list.items[0]->kind==N_SYMBOL && strncmp(f->as.list.items[0]->as.sym.ptr,"def",3)==0) {
      Node *nm=f->as.list.items[1]; if (nm->kind!=N_SYMBOL) continue;
      if (strncmp(nm->as.sym.ptr,"main",4)==0) return (int)i;
    }
  }
  return -1;
}

static void emit_header(Str *s, const CodegenOpts *opts){
  append(s, "; ModuleID = 'sqale'\nsource_filename = ");
  append_quoted(s, opts && opts->module_name ? opts->module_name : "sqale");
  append(s, "\n\n");
  append(s, "declare void @sq_print_i64(i64)\n");
  append(s, "declare void @sq_print_cstr(i8*)\n\n");
}

static void emit_print(Str *s, Str *globals, Node *arg, int *tmp_id){
  if (arg->kind==N_INT) {
    append(s, "  call void @sq_print_i64(i64 ");
    char buf[64]; snprintf(buf,sizeof(buf),"%lld", (long long)arg->as.ival);
    append(s, buf); append(s, ")\n");
  } else if (arg->kind==N_STRING) {
    int id = (*tmp_id)++;
    char lenbuf[32]; snprintf(lenbuf,sizeof(lenbuf),"%zu", arg->as.str.len+1);
    // global
    append(globals, "@.str"); char b[16]; snprintf(b,sizeof(b),"%d",id); append(globals,b);
    append(globals, " = private unnamed_addr constant ["); append(globals,lenbuf); append(globals, " x i8] c\"");
    str_append_n(globals, arg->as.str.ptr, arg->as.str.len);
    append(globals, "\\00\"\n");
    // body
    append(s, "  %ptr"); append(s,b);
    append(s, " = getelementptr inbounds (["); append(s,lenbuf); append(s, " x i8], ["); append(s,lenbuf); append(s, " x i8]* @.str"); append(s,b); append(s, ", i64 0, i64 0)\n");
    append(s, "  call void @sq_print_cstr(i8* %ptr"); append(s,b); append(s, ")\n");
  }
  else if (arg->kind==N_LIST && arg->as.list.count==3 && arg->as.list.items[0]->kind==N_SYMBOL && strncmp(arg->as.list.items[0]->as.sym.ptr, "+",1)==0) {
    Node *a = arg->as.list.items[1]; Node *b = arg->as.list.items[2];
    if (a->kind==N_INT && b->kind==N_INT) {
      int id = (*tmp_id)++;
      append(s, "  %t"); char bnr[16]; snprintf(bnr,sizeof(bnr),"%d",id); append(s,bnr);
      append(s, " = add i64 ");
      char abuf[64]; snprintf(abuf,sizeof(abuf),"%lld", (long long)a->as.ival);
      char bbuf[64]; snprintf(bbuf,sizeof(bbuf),"%lld", (long long)b->as.ival);
      append(s, abuf); append(s, ", "); append(s, bbuf); append(s, "\n");
      append(s, "  call void @sq_print_i64(i64 %t"); append(s,bnr); append(s, ")\n");
    }
  }
  else if (arg->kind==N_LIST && arg->as.list.count==3 && arg->as.list.items[0]->kind==N_SYMBOL && strncmp(arg->as.list.items[0]->as.sym.ptr, "-",1)==0) {
    Node *a = arg->as.list.items[1]; Node *b = arg->as.list.items[2];
    if (a->kind==N_INT && b->kind==N_INT) {
      int id = (*tmp_id)++;
      append(s, "  %t"); char bnr[16]; snprintf(bnr,sizeof(bnr),"%d",id); append(s,bnr);
      append(s, " = sub i64 ");
      char abuf[64]; snprintf(abuf,sizeof(abuf),"%lld", (long long)a->as.ival);
      char bbuf[64]; snprintf(bbuf,sizeof(bbuf),"%lld", (long long)b->as.ival);
      append(s, abuf); append(s, ", "); append(s, bbuf); append(s, "\n");
      append(s, "  call void @sq_print_i64(i64 %t"); append(s,bnr); append(s, ")\n");
    }
  }
}

char *codegen_emit_ir(Node *program, const CodegenOpts *opts, size_t *out_len) {
#if USE_LLVM
  (void)program; (void)opts;
  LLVMModuleRef mod = LLVMModuleCreateWithName(opts && opts->module_name ? opts->module_name : "sqale");
  LLVMTypeRef i32 = LLVMInt32Type();
  LLVMTypeRef fn_ty = LLVMFunctionType(i32, NULL, 0, 0);
  LLVMValueRef fn = LLVMAddFunction(mod, "main", fn_ty);
  LLVMBasicBlockRef entry = LLVMAppendBasicBlock(fn, "entry");
  LLVMBuilderRef b = LLVMCreateBuilder();
  LLVMPositionBuilderAtEnd(b, entry);
  LLVMBuildRet(b, LLVMConstInt(i32, 0, 0));
  char *ir = LLVMPrintModuleToString(mod);
  LLVMDisposeBuilder(b);
  LLVMDisposeModule(mod);
  if (out_len) *out_len = strlen(ir);
  return ir; // LLVM allocated memory via malloc; caller frees with free()
#else
  Str s; str_init(&s); Str g; str_init(&g);
  emit_header(&s, opts);
  int mi = find_main_fn(program);
  if (mi>=0 && opts && opts->for_exe) {
    append(&s, "define i32 @main() {\n");
    Node *def = program->as.list.items[mi];
    Node *fn = def->as.list.items[4];
    int tmp_id = 0;
    for (size_t i=2;i<fn->as.list.count;i++) {
      Node *e = fn->as.list.items[i];
      if (e->kind==N_LIST && e->as.list.count>=2 && e->as.list.items[0]->kind==N_SYMBOL && strncmp(e->as.list.items[0]->as.sym.ptr,"print",5)==0) {
        if (e->as.list.count==2) emit_print(&s, &g, e->as.list.items[1], &tmp_id);
      }
    }
    int64_t retv = 0; int found=0;
    for (size_t i=fn->as.list.count; i>0; i--) {
      Node *e = fn->as.list.items[i-1];
      if (e->kind==N_INT) { retv = e->as.ival; found=1; break; }
    }
    char buf[64]; snprintf(buf,sizeof(buf),"%lld", (long long)(found?retv:0));
    append(&s, "  ret i32 "); append(&s, buf); append(&s, "\n}\n");
  } else {
    append(&s, "define i32 @main() {\n  ret i32 0\n}\n");
  }
  // Prepend globals
  Str out; str_init(&out); str_append(&out, g.data?g.data:""); str_append(&out, s.data?s.data:"");
  if (out_len) *out_len = out.len;
  free(s.data); free(g.data);
  return out.data; // transfer ownership
#endif
}
