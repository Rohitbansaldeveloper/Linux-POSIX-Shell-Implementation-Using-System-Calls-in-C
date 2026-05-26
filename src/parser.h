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
    int fd_redirs_count;
    struct {
        int source_fd;
        int target_fd;
    } fd_redirs[4];
    char *heredoc_delimiter;
} Command;

typedef struct {
    Command commands[MAX_COMMANDS];
    int num_commands;
    int background;
} Pipeline;

typedef enum {
    NODE_PIPELINE,
    NODE_AND,
    NODE_OR,
    NODE_IF,
    NODE_WHILE,
    NODE_SEQUENCE
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    union {
        Pipeline pipeline;
        struct {
            struct ASTNode *left;
            struct ASTNode *right;
        } binary;
        struct {
            struct ASTNode *condition;
            struct ASTNode *then_branch;
            struct ASTNode *else_branch;
        } if_stmt;
        struct {
            struct ASTNode *condition;
            struct ASTNode *body;
        } while_stmt;
    } data;
} ASTNode;

ASTNode *parse(Token *tokens);

#endif
