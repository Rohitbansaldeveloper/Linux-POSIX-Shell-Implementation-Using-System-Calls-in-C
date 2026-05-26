#include "parser.h"
#include "memory.h"
#include "string_utils.h"

static int pos = 0;
static Token *tks;

static ASTNode *parse_statement(void);
static ASTNode *parse_logical(void);
static ASTNode *parse_pipeline(void);

static Token *peek(void) {
    return &tks[pos];
}

static Token *consume(void) {
    return &tks[pos++];
}

static ASTNode *parse_pipeline(void) {
    if (peek()->type == TOKEN_EOF || peek()->type == TOKEN_SEMI) {
        return NULL;
    }

    ASTNode *node = mem_alloc_temp(sizeof(ASTNode));
    if (!node) return NULL;
    node->type = NODE_PIPELINE;
    Pipeline *p = &node->data.pipeline;
    p->num_commands = 0;
    p->background = 0;
    
    Command *cmd = &p->commands[0];
    cmd->argc = 0;
    cmd->redirect_in = NULL;
    cmd->redirect_out = NULL;
    cmd->append_out = 0;
    cmd->fd_redirs_count = 0;
    cmd->heredoc_delimiter = NULL;
    p->num_commands = 1;
    
    while (peek()->type != TOKEN_EOF && 
           peek()->type != TOKEN_AND && 
           peek()->type != TOKEN_OR && 
           peek()->type != TOKEN_THEN && 
           peek()->type != TOKEN_ELSE && 
           peek()->type != TOKEN_FI && 
           peek()->type != TOKEN_DO && 
           peek()->type != TOKEN_DONE &&
           peek()->type != TOKEN_SEMI) {
        
        Token *t = consume();
        
        if (t->type == TOKEN_WORD) {
            if (cmd->argc < MAX_ARGS - 1) cmd->argv[cmd->argc++] = t->value;
        } else if (t->type == TOKEN_PIPE) {
            cmd->argv[cmd->argc] = NULL;
            cmd = &p->commands[p->num_commands++];
            cmd->argc = 0;
            cmd->redirect_in = NULL;
            cmd->redirect_out = NULL;
            cmd->append_out = 0;
            cmd->fd_redirs_count = 0;
            cmd->heredoc_delimiter = NULL;
            cmd->fd_redirs_count = 0;
        } else if (t->type == TOKEN_REDIRECT_IN) {
            if (peek()->type == TOKEN_WORD) cmd->redirect_in = consume()->value;
        } else if (t->type == TOKEN_REDIRECT_OUT || t->type == TOKEN_REDIRECT_APPEND) {
            if (peek()->type == TOKEN_WORD) {
                cmd->redirect_out = consume()->value;
                cmd->append_out = (t->type == TOKEN_REDIRECT_APPEND);
            }
        } else if (t->type == TOKEN_REDIRECT_STDERR) {
            if (cmd->fd_redirs_count < 4) {
                int source = 0;
                int target = 0;
                int i = 0;
                while (t->value[i] >= '0' && t->value[i] <= '9') {
                    source = source * 10 + (t->value[i] - '0');
                    i++;
                }
                while (t->value[i] == '>' || t->value[i] == '&' || t->value[i] == '<') i++;
                while (t->value[i] >= '0' && t->value[i] <= '9') {
                    target = target * 10 + (t->value[i] - '0');
                    i++;
                }
                cmd->fd_redirs[cmd->fd_redirs_count].source_fd = source;
                cmd->fd_redirs[cmd->fd_redirs_count].target_fd = target;
                cmd->fd_redirs_count++;
            }
        } else if (t->type == TOKEN_HEREDOC) {
            if (peek()->type == TOKEN_WORD) cmd->heredoc_delimiter = consume()->value;
        } else if (t->type == TOKEN_BACKGROUND) {
            p->background = 1;
            break;
        }
    }
    cmd->argv[cmd->argc] = NULL;
    
    if (p->num_commands == 1 && p->commands[0].argc == 0) {
        return NULL; // Empty pipeline
    }
    return node;
}

static ASTNode *parse_statement(void) {
    if (peek()->type == TOKEN_IF) {
        consume(); // skip 'if'
        ASTNode *node = mem_alloc_temp(sizeof(ASTNode));
        if (!node) return NULL;
        node->type = NODE_IF;
        node->data.if_stmt.condition = parse_logical();
        
        if (peek()->type == TOKEN_SEMI) consume();
        if (peek()->type == TOKEN_THEN) consume();
        
        node->data.if_stmt.then_branch = parse_logical();
        
        if (peek()->type == TOKEN_SEMI) consume();
        
        if (peek()->type == TOKEN_ELSE) {
            consume();
            node->data.if_stmt.else_branch = parse_logical();
        } else {
            node->data.if_stmt.else_branch = NULL;
        }
        
        if (peek()->type == TOKEN_SEMI) consume();
        if (peek()->type == TOKEN_FI) consume();
        
        return node;
    } else if (peek()->type == TOKEN_WHILE) {
        consume(); // skip 'while'
        ASTNode *node = mem_alloc_temp(sizeof(ASTNode));
        if (!node) return NULL;
        node->type = NODE_WHILE;
        node->data.while_stmt.condition = parse_logical();
        
        if (peek()->type == TOKEN_SEMI) consume();
        if (peek()->type == TOKEN_DO) consume();
        
        node->data.while_stmt.body = parse_logical();
        
        if (peek()->type == TOKEN_SEMI) consume();
        if (peek()->type == TOKEN_DONE) consume();
        
        return node;
    }
    
    return parse_pipeline();
}

static ASTNode *parse_logical(void) {
    ASTNode *left = parse_statement();
    if (!left) return NULL;
    
    while (peek()->type == TOKEN_AND || peek()->type == TOKEN_OR || peek()->type == TOKEN_SEMI) {
        Token *op = consume();
        ASTNode *right = parse_statement();
        if (!right) break;
        
        ASTNode *new_node = mem_alloc_temp(sizeof(ASTNode));
        if (!new_node) return left;
        
        if (op->type == TOKEN_AND) new_node->type = NODE_AND;
        else if (op->type == TOKEN_OR) new_node->type = NODE_OR;
        else new_node->type = NODE_SEQUENCE;
        
        new_node->data.binary.left = left;
        new_node->data.binary.right = right;
        left = new_node;
    }
    return left;
}

ASTNode *parse(Token *tokens) {
    pos = 0;
    tks = tokens;
    if (peek()->type == TOKEN_EOF) return NULL;
    return parse_logical();
}
