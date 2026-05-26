#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "syscalls.h"

#define MAX_TOKENS 128
#define MAX_TOKEN_LEN 256

typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,     // |
    TOKEN_REDIRECT_IN,  // <
    TOKEN_REDIRECT_OUT, // >
    TOKEN_REDIRECT_APPEND, // >>
    TOKEN_BACKGROUND, // &
    TOKEN_REDIRECT_STDERR, // 2>&1
    TOKEN_HEREDOC, // <<
    TOKEN_AND,      // &&
    TOKEN_OR,       // ||
    TOKEN_IF,       // if
    TOKEN_THEN,     // then
    TOKEN_ELSE,     // else
    TOKEN_FI,       // fi
    TOKEN_WHILE,    // while
    TOKEN_DO,       // do
    TOKEN_DONE,     // done
    TOKEN_SEMI,     // ;
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    char value[MAX_TOKEN_LEN];
} Token;

int tokenize(const char *input, Token *tokens, int max_tokens);

#endif
