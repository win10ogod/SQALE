#include "value.h"

Value v_int(int64_t x){ Value v; v.kind=VAL_INT; v.as.i=x; return v; }
Value v_float(double x){ Value v; v.kind=VAL_FLOAT; v.as.f=x; return v; }
Value v_bool(bool x){ Value v; v.kind=VAL_BOOL; v.as.b=x; return v; }
Value v_unit(void){ Value v; v.kind=VAL_UNIT; return v; }
Value v_str(String *s){ Value v; v.kind=VAL_STR; v.as.str=s; return v; }
Value v_native(NativeFn f, Type *type){ Value v; v.kind=VAL_FUNC; v.as.native.fn=f; v.as.native.type=type; return v; }
Value v_closure(Closure *c){ Value v; v.kind=VAL_CLOSURE; v.as.clos=c; return v; }
Value v_chan(Channel *c){ Value v; v.kind=VAL_CHAN; v.as.chan=c; return v; }
Value v_symbol(const char *name, int32_t len){ Value v; v.kind=VAL_SYMBOL; v.as.sym.name=name; v.as.sym.len=len; return v; }
Value v_list(ValList *l){ Value v; v.kind=VAL_LIST; v.as.list=l; return v; }
Value v_vec(Vector *vec){ Value v; v.kind=VAL_VEC; v.as.vec=vec; return v; }
Value v_map(Map *m){ Value v; v.kind=VAL_MAP; v.as.map=m; return v; }
