/*
 * parser.c
 * 
 * The parser's responsibility is to convert a linear stream of tokens
 * (produced by the tokenizer) into a structured Abstract Syntax Tree (AST),
 * which for this simple shell is represented by the 'Pipeline' structure.
 * 
 * It groups words into commands, detects pipes ('|') to separate commands,
 * and extracts file redirections ('<', '>', '>>').
 */

#include "parser.h"
#include "string_utils.h"

int parse(Token *tokens, Pipeline *pipeline) {
    pipeline->num_commands = 0;
    pipeline->background = 0;
    
    if (tokens[0].type == TOKEN_EOF) {
        return 0; // Empty input
    }
    
    // Initialize the first command in the pipeline
    Command *current_cmd = &pipeline->commands[0];
    current_cmd->argc = 0;
    current_cmd->redirect_in = NULL;
    current_cmd->redirect_out = NULL;
    current_cmd->append_out = 0;
    current_cmd->merge_stderr = 0;
    current_cmd->heredoc_delimiter = NULL;
    pipeline->num_commands = 1;
    
    // Iterate over all tokens until EOF
    for (int i = 0; tokens[i].type != TOKEN_EOF; i++) {
        Token *t = &tokens[i];
        
        if (t->type == TOKEN_WORD) {
            // A regular word is treated as a command argument
            if (current_cmd->argc < MAX_ARGS - 1) {
                current_cmd->argv[current_cmd->argc++] = t->value;
            }
        } else if (t->type == TOKEN_PIPE) {
            // A pipe indicates the end of the current command and the start of a new one.
            if (pipeline->num_commands < MAX_COMMANDS) {
                current_cmd->argv[current_cmd->argc] = NULL; // NULL-terminate argv array for execve
                
                // Move to the next command slot in the pipeline array
                current_cmd = &pipeline->commands[pipeline->num_commands++];
                current_cmd->argc = 0;
                current_cmd->redirect_in = NULL;
                current_cmd->redirect_out = NULL;
                current_cmd->append_out = 0;
                current_cmd->merge_stderr = 0;
                current_cmd->heredoc_delimiter = NULL;
            } else {
                return -1; // Exceeded maximum allowed commands in a single pipeline
            }
        } else if (t->type == TOKEN_REDIRECT_IN) {
            // '<' indicates the next token is the input file
            if (tokens[i+1].type == TOKEN_WORD) {
                current_cmd->redirect_in = tokens[i+1].value;
                i++; // Skip the filename token so we don't process it as an argument
            } else {
                return -1; // Syntax error: missing filename after '<'
            }
        } else if (t->type == TOKEN_REDIRECT_OUT || t->type == TOKEN_REDIRECT_APPEND) {
            // '>' or '>>' indicates the next token is the output file
            if (tokens[i+1].type == TOKEN_WORD) {
                current_cmd->redirect_out = tokens[i+1].value;
                current_cmd->append_out = (t->type == TOKEN_REDIRECT_APPEND);
                i++; // Skip the filename token
            } else {
                return -1; // Syntax error: missing filename after '>'
            }
        } else if (t->type == TOKEN_REDIRECT_STDERR) {
            current_cmd->merge_stderr = 1;
        } else if (t->type == TOKEN_HEREDOC) {
            if (tokens[i+1].type == TOKEN_WORD) {
                current_cmd->heredoc_delimiter = tokens[i+1].value;
                i++; // Skip delimiter
            } else {
                return -1; // Syntax error
            }
        } else if (t->type == TOKEN_BACKGROUND) {
            // '&' puts the entire pipeline into the background.
            // In a strict POSIX grammar, this usually appears at the very end.
            pipeline->background = 1;
            break;
        }
    }
    
    // Ensure the argv array of the very last command is NULL-terminated.
    // The execve system call requires a NULL pointer at the end of the arguments array.
    current_cmd->argv[current_cmd->argc] = NULL;
    return 1;
}
