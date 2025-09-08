#ifndef PARSER_H
#define PARSER_H

#include "token.h"
#include "ast.h"
#include "arena.h"

typedef struct Parser {
  Lexer lx;
  Token cur;
  Token peek;
  Arena *arena;
  const char *src;
  size_t len;
} Parser;

void parser_init(Parser *p, Arena *arena, const char *src, size_t len);
Node *parse_toplevel(Parser *p);

#endif // PARSER_H

