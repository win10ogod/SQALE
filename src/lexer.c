#include "token.h"
#include <ctype.h>
#include <string.h>

static int is_sym_start(int c) {
  return isalpha(c) || c=='_' || strchr("+-*/<>=!?%", c)!=0;
}
static int is_sym_part(int c) {
  return isalnum(c) || c=='_' || strchr("+-*/<>=!?%", c)!=0;
}

void lexer_init(Lexer *lx, const char *src, size_t len) {
  lx->src = src; lx->len = len; lx->pos = 0; lx->line = 1; lx->col = 1;
}

static char peek(Lexer *lx) { return (lx->pos < lx->len) ? lx->src[lx->pos] : '\0'; }
static char peek2(Lexer *lx) { return (lx->pos+1 < lx->len) ? lx->src[lx->pos+1] : '\0'; }
static char advance(Lexer *lx) {
  if (lx->pos >= lx->len) return '\0';
  char c = lx->src[lx->pos++];
  if (c=='\n') { lx->line++; lx->col=1; } else { lx->col++; }
  return c;
}

static void skip_ws_comments(Lexer *lx) {
  for (;;) {
    char c = peek(lx);
    if (c==';' ) { // line comment
      while (peek(lx) && peek(lx)!='\n') advance(lx);
      continue;
    }
    if (isspace((unsigned char)c)) { advance(lx); continue; }
    break;
  }
}

static Token make(Lexer *lx, TokenKind k, const char *start, size_t len, size_t line, size_t col) {
  (void)lx; Token t; t.kind=k; t.lexeme=start; t.len=len; t.line=line; t.col=col; return t;
}

Token lexer_next(Lexer *lx) {
  skip_ws_comments(lx);
  size_t line = lx->line, col = lx->col;
  char c = peek(lx);
  if (!c) return make(lx, T_EOF, lx->src + lx->pos, 0, line, col);
  if (c=='[') { advance(lx); return make(lx, T_LBRACK, lx->src+lx->pos-1, 1, line, col); }
  if (c==']') { advance(lx); return make(lx, T_RBRACK, lx->src+lx->pos-1, 1, line, col); }
  if (c==':') { advance(lx); return make(lx, T_COLON, lx->src+lx->pos-1, 1, line, col); }
  if (c=='-' && peek2(lx)=='>') {
    advance(lx); advance(lx);
    return make(lx, T_ARROW, lx->src+lx->pos-2, 2, line, col);
  }
  if (c=='"') {
    advance(lx); // consume opening quote
    const char *start = lx->src + lx->pos;
    size_t len = 0;
    while ((c = peek(lx)) && c!='"') {
      if (c=='\\') { advance(lx); if (!peek(lx)) break; advance(lx); len+=2; continue; }
      advance(lx); len++;
    }
    if (peek(lx)=='"') advance(lx);
    return make(lx, T_STRING, start, len, line, col);
  }
  if (isdigit((unsigned char)c) || (c=='-' && isdigit((unsigned char)peek2(lx)))) {
    const char *start = lx->src + lx->pos;
    size_t start_pos = lx->pos;
    int seen_dot = 0;
    if (c=='-') advance(lx);
    while (isdigit((unsigned char)peek(lx))) advance(lx);
    if (peek(lx)=='.' && isdigit((unsigned char)peek2(lx))) {
      seen_dot = 1; advance(lx);
      while (isdigit((unsigned char)peek(lx))) advance(lx);
    }
    size_t end_pos = lx->pos;
    return make(lx, seen_dot ? T_FLOAT : T_INT, start, end_pos - start_pos, line, col);
  }
  if (is_sym_start((unsigned char)c)) {
    const char *start = lx->src + lx->pos;
    size_t start_pos = lx->pos;
    advance(lx);
    while (is_sym_part((unsigned char)peek(lx))) advance(lx);
    size_t end_pos = lx->pos;
    return make(lx, T_SYMBOL, start, end_pos - start_pos, line, col);
  }
  // Unknown char -> error token consuming one char
  advance(lx);
  return make(lx, T_ERROR, lx->src + lx->pos - 1, 1, line, col);
}
