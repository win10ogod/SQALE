#include "type.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Type *mk(void *arena, TypeKind k) {
  (void)arena; // Currently malloc; could switch to arena later
  Type *t = (Type*)malloc(sizeof(Type));
  t->kind = k;
  return t;
}

Type *ty_int(void *arena)   { return mk(arena, TY_INT); }
Type *ty_float(void *arena) { return mk(arena, TY_FLOAT); }
Type *ty_bool(void *arena)  { return mk(arena, TY_BOOL); }
Type *ty_str(void *arena)   { return mk(arena, TY_STR); }
Type *ty_unit(void *arena)  { return mk(arena, TY_UNIT); }
Type *ty_any(void *arena)   { return mk(arena, TY_ANY); }
Type *ty_error(void *arena) { return mk(arena, TY_ERROR); }

Type *ty_func(void *arena, Type **params, size_t arity, Type *ret) {
  Type *t = mk(arena, TY_FUNC);
  t->as.fn.params = (Type**)malloc(sizeof(Type*)*arity);
  for (size_t i=0;i<arity;i++) t->as.fn.params[i]=params[i];
  t->as.fn.arity = arity; t->as.fn.ret = ret; return t;
}

Type *ty_chan(void *arena, Type *elem) {
  Type *t = mk(arena, TY_CHAN); t->as.chan.elem=elem; return t;
}

Type *ty_vec(void *arena, Type *elem) {
  Type *t = mk(arena, TY_VEC); t->as.chan.elem=elem; return t; }
Type *ty_map(void *arena, Type *key, Type *val) {
  // Reuse fn struct for pair to keep code tiny (not ideal)
  Type *t = mk(arena, TY_MAP); t->as.fn.params = (Type**)malloc(sizeof(Type*)*2); t->as.fn.params[0]=key; t->as.fn.params[1]=val; t->as.fn.arity=2; t->as.fn.ret=NULL; return t; }

bool ty_eq(const Type *a, const Type *b) {
  if (a==b) return true;
  if (!a || !b) return false;
  if (a->kind != b->kind) {
    // ANY is compatible with anything
    if (a->kind==TY_ANY || b->kind==TY_ANY) return true;
    return false;
  }
  switch (a->kind) {
    case TY_FUNC:
      if (!ty_eq(a->as.fn.ret, b->as.fn.ret)) return false;
      if (a->as.fn.arity != b->as.fn.arity) return false;
      for (size_t i=0;i<a->as.fn.arity;i++) if (!ty_eq(a->as.fn.params[i], b->as.fn.params[i])) return false;
      return true;
    case TY_CHAN:
      return ty_eq(a->as.chan.elem, b->as.chan.elem);
    case TY_VEC:
      return ty_eq(a->as.chan.elem, b->as.chan.elem);
    case TY_MAP:
      return ty_eq(a->as.fn.params[0], b->as.fn.params[0]) && ty_eq(a->as.fn.params[1], b->as.fn.params[1]);
    default:
      return true;
  }
}

const char *ty_kind_name(TypeKind k) {
  switch (k) {
    case TY_INT: return "Int";
    case TY_FLOAT: return "Float";
    case TY_BOOL: return "Bool";
    case TY_STR: return "Str";
    case TY_UNIT: return "Unit";
    case TY_FUNC: return "Func";
    case TY_ANY: return "Any";
    case TY_CHAN: return "Chan";
    case TY_ERROR: return "Error";
    case TY_VEC: return "Vec";
    case TY_MAP: return "Map";
  }
  return "?";
}

void ty_to_string(const Type *t, char *buf, size_t bufsize) {
  if (!t) { snprintf(buf, bufsize, "<null>"); return; }
  switch (t->kind) {
    case TY_INT: snprintf(buf, bufsize, "Int"); break;
    case TY_FLOAT: snprintf(buf, bufsize, "Float"); break;
    case TY_BOOL: snprintf(buf, bufsize, "Bool"); break;
    case TY_STR: snprintf(buf, bufsize, "Str"); break;
    case TY_UNIT: snprintf(buf, bufsize, "Unit"); break;
    case TY_ANY: snprintf(buf, bufsize, "Any"); break;
    case TY_CHAN: {
      char tmp[128]; ty_to_string(t->as.chan.elem, tmp, sizeof(tmp));
      snprintf(buf, bufsize, "(Chan %s)", tmp); break; }
    case TY_FUNC: {
      char tmp[256]; size_t pos=0; pos += snprintf(tmp+pos, sizeof(tmp)-pos, "(");
      for (size_t i=0;i<t->as.fn.arity;i++) {
        char pbuf[64]; ty_to_string(t->as.fn.params[i], pbuf, sizeof(pbuf));
        pos += snprintf(tmp+pos, sizeof(tmp)-pos, "%s%s", i?" ":"", pbuf);
      }
      char rbuf[64]; ty_to_string(t->as.fn.ret, rbuf, sizeof(rbuf));
      pos += snprintf(tmp+pos, sizeof(tmp)-pos, " -> %s)", rbuf);
      snprintf(buf, bufsize, "%s", tmp); break; }
    case TY_ERROR: snprintf(buf, bufsize, "<type-error>"); break;
    case TY_VEC: { char tmp[64]; ty_to_string(t->as.chan.elem,tmp,sizeof(tmp)); snprintf(buf,bufsize,"(Vec %s)",tmp); break; }
    case TY_MAP: { char k[32],v[32]; ty_to_string(t->as.fn.params[0],k,sizeof(k)); ty_to_string(t->as.fn.params[1],v,sizeof(v)); snprintf(buf,bufsize,"(Map %s %s)",k,v); break; }
  }
}
