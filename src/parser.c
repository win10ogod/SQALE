#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void advance(Parser *p) { p->cur = p->peek; p->peek = lexer_next(&p->lx); }

void parser_init(Parser *p, Arena *arena, const char *src, size_t len) {
  p->arena = arena; p->src=src; p->len=len;
  lexer_init(&p->lx, src, len);
  p->cur = lexer_next(&p->lx);
  p->peek = lexer_next(&p->lx);
}

static Node *parse_form(Parser *p);

static Node *parse_list(Parser *p) {
  size_t line=p->cur.line, col=p->cur.col;
  // assume current token is T_LBRACK
  advance(p); // consume [
  Node *list = node_new_list(p->arena, 4);
  list->line = line; list->col = col;
  while (p->cur.kind != T_RBRACK && p->cur.kind != T_EOF) {
    Node *elem = parse_form(p);
    if (!elem) break;
    node_list_push(p->arena, list, elem);
  }
  if (p->cur.kind == T_RBRACK) advance(p); // consume ]
  return list;
}

static int is_kw(const char *s, size_t n, const char *kw) {
  return strlen(kw)==n && strncmp(s, kw, n)==0;
}

static Node *parse_form(Parser *p) {
  Token t = p->cur;
  switch (t.kind) {
    case T_LBRACK: return parse_list(p);
    case T_COLON: {
      Node *n = node_new_symbol(p->arena, ":", 1, t.line, t.col);
      advance(p); return n;
    }
    case T_ARROW: {
      Node *n = node_new_symbol(p->arena, "->", 2, t.line, t.col);
      advance(p); return n;
    }
    case T_INT: {
      char tmp[64]; size_t n = t.len<63?t.len:63; memcpy(tmp, t.lexeme, n); tmp[n]='\0';
      int64_t v = strtoll(tmp, NULL, 10);
      advance(p); return node_new_int(p->arena, v, t.line, t.col);
    }
    case T_FLOAT: {
      char *buf = (char*)malloc(t.len+1);
      memcpy(buf, t.lexeme, t.len); buf[t.len]='\0';
      double v = strtod(buf, NULL); free(buf);
      advance(p); return node_new_float(p->arena, v, t.line, t.col);
    }
    case T_STRING: {
      // Note: string escapes are not unescaped here for simplicity
      Node *n = node_new_string(p->arena, t.lexeme, t.len, t.line, t.col);
      advance(p); return n;
    }
    case T_SYMBOL: {
      bool b;
      if (is_kw(t.lexeme, t.len, "true")) { b=true; advance(p); return node_new_bool(p->arena, b, t.line, t.col); }
      if (is_kw(t.lexeme, t.len, "false")) { b=false; advance(p); return node_new_bool(p->arena, b, t.line, t.col); }
      Node *n = node_new_symbol(p->arena, t.lexeme, t.len, t.line, t.col);
      advance(p); return n;
    }
    case T_RBRACK: // unexpected
    case T_EOF:
    default:
      advance(p);
      return NULL;
  }
}

Node *parse_toplevel(Parser *p) {
  Node *top = node_new_list(p->arena, 8);
  while (p->cur.kind != T_EOF) {
    Node *f = parse_form(p);
    if (f) node_list_push(p->arena, top, f);
  }
  return top;
}
