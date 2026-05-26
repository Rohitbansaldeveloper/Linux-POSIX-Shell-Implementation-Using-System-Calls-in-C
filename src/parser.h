#ifndef PARSER_H
#define PARSER_H

#include "tokenizer.h"

#define MAX_ARGS 64
#define MAX_COMMANDS 16

typedef struct {
    char *argv[MAX_ARGS];
    int argc;
    char *redirect_in;
    char *redirect_out;
    int append_out;
    int merge_stderr;       // 1 if 2>&1
    char *heredoc_delimiter; // delimiter if <<
} Command;

typedef struct {
    Command commands[MAX_COMMANDS];
    int num_commands;
    int background;
} Pipeline;

int parse(Token *tokens, Pipeline *pipeline);

#endif
