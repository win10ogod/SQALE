#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
  T_LBRACK,
  T_RBRACK,
  T_SYMBOL,
  T_INT,
  T_FLOAT,
  T_STRING,
  T_COLON,      // :
  T_ARROW,      // ->
  T_EOF,
  T_ERROR
} TokenKind;

typedef struct {
  TokenKind kind;
  const char *lexeme; // pointer into source buffer slice
  size_t len;
  size_t line;
  size_t col;
} Token;

typedef struct {
  const char *src;
  size_t len;
  size_t pos;
  size_t line;
  size_t col;
} Lexer;

void lexer_init(Lexer *lx, const char *src, size_t len);
Token lexer_next(Lexer *lx);

#endif // TOKEN_H

