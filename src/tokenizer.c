/*
 * tokenizer.c
 * 
 * The tokenizer (or lexer) performs lexical analysis. It takes a raw string
 * of characters typed by the user and breaks it down into meaningful units
 * called 'Tokens' (words, pipes, redirection symbols).
 */

#include "tokenizer.h"
#include "string_utils.h"
#include "env.h"
#include "memory.h"

extern void execute_string(const char *str, int out_fd);

// Helper to identify whitespace characters
static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

// Helper to identify special shell characters that should be their own tokens
static int is_special(char c) {
    return c == '|' || c == '<' || c == '>' || c == '&' || c == ';' || c == '\n';
}

int tokenize(const char *input, Token *tokens, int max_tokens) {
    int count = 0;
    
    while (*input && count < max_tokens - 1) {
        // 1. Skip leading whitespace
        while (is_space(*input)) {
            input++;
        }
        if (!*input) break;
        
        // 2. Identify tokens
        if (*input == '2' && *(input+1) == '>' && *(input+2) == '&' && *(input+3) == '1') {
            tokens[count].type = TOKEN_REDIRECT_STDERR;
            tokens[count].value[0] = '2';
            tokens[count].value[1] = '>';
            tokens[count].value[2] = '&';
            tokens[count].value[3] = '1';
            tokens[count].value[4] = '\0';
            input += 4;
            count++;
        } else if (*input == '|') {
            if (*(input + 1) == '|') {
                tokens[count].type = TOKEN_OR;
                tokens[count].value[0] = '|';
                tokens[count].value[1] = '|';
                tokens[count].value[2] = '\0';
                input += 2;
            } else {
                tokens[count].type = TOKEN_PIPE;
                tokens[count].value[0] = '|';
                tokens[count].value[1] = '\0';
                input++;
            }
            count++;
        } else if (*input == '<') {
            if (*(input + 1) == '<') {
                tokens[count].type = TOKEN_HEREDOC;
                tokens[count].value[0] = '<';
                tokens[count].value[1] = '<';
                tokens[count].value[2] = '\0';
                input += 2;
            } else {
                tokens[count].type = TOKEN_REDIRECT_IN;
                tokens[count].value[0] = '<';
                tokens[count].value[1] = '\0';
                input++;
            }
            count++;
        } else if (*input == '>') {
            // Check for double chevron '>>' (append mode)
            if (*(input + 1) == '>') {
                tokens[count].type = TOKEN_REDIRECT_APPEND;
                tokens[count].value[0] = '>';
                tokens[count].value[1] = '>';
                tokens[count].value[2] = '\0';
                input += 2;
            } else {
                tokens[count].type = TOKEN_REDIRECT_OUT;
                tokens[count].value[0] = '>';
                tokens[count].value[1] = '\0';
                input++;
            }
            count++;
        } else if (*input == '&') {
            if (*(input + 1) == '&') {
                tokens[count].type = TOKEN_AND;
                tokens[count].value[0] = '&';
                tokens[count].value[1] = '&';
                tokens[count].value[2] = '\0';
                input += 2;
            } else {
                tokens[count].type = TOKEN_BACKGROUND;
                tokens[count].value[0] = '&';
                tokens[count].value[1] = '\0';
                input++;
            }
            count++;
        } else if (*input == ';' || *input == '\n') {
            tokens[count].type = TOKEN_SEMI;
            tokens[count].value[0] = ';';
            tokens[count].value[1] = '\0';
            input++;
            count++;
        } else {
            // Check for command substitution $(...)
            if (*input == '$' && *(input+1) == '(') {
                input += 2;
                char sub_cmd[512];
                int sub_i = 0;
                while (*input && *input != ')') {
                    sub_cmd[sub_i++] = *input++;
                }
                if (*input == ')') input++; // skip )
                sub_cmd[sub_i] = '\0';
                
                int p[2];
                if (sys_pipe(p) == 0) {
                    pid_t pid = sys_fork();
                    if (pid == 0) {
                        sys_close(p[0]);
                        execute_string(sub_cmd, p[1]);
                        sys_exit(0);
                    } else if (pid > 0) {
                        sys_close(p[1]);
                        char buf[1024];
                        int n = sys_read(p[0], buf, sizeof(buf)-1);
                        if (n > 0) {
                            while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) n--;
                            buf[n] = '\0';
                            str_cpy(tokens[count].value, buf);
                        } else {
                            tokens[count].value[0] = '\0';
                        }
                        sys_close(p[0]);
                        int status;
                        sys_wait4(pid, &status, 0, NULL);
                    }
                }
                tokens[count].type = TOKEN_WORD;
                count++;
                continue;
            }

            // If it's not a special character, it's a regular word (command or argument)
            tokens[count].type = TOKEN_WORD;
            int i = 0;
            
            // Minimal quote handling: if we see a double quote, we ignore spaces 
            // and special characters until the closing quote is found.
            int in_quote = 0;
            
            while (*input && i < MAX_TOKEN_LEN - 1) {
                if (*input == '"') {
                    in_quote = !in_quote; // Toggle quote state
                    input++;
                    continue; // Skip the quote character itself
                }
                
                // If we are not inside quotes, space or special chars terminate the word
                if (!in_quote && (is_space(*input) || is_special(*input))) {
                    break;
                }
                
                tokens[count].value[i++] = *input++;
            }
            tokens[count].value[i] = '\0'; // Null terminate the word string
            
            // Check for variable expansion (e.g. $HOME)
            if (tokens[count].value[0] == '$' && tokens[count].value[1] != '\0') {
                char *val = get_env_val(&tokens[count].value[1]);
                if (val) {
                    str_cpy(tokens[count].value, val);
                } else {
                    tokens[count].value[0] = '\0'; // Empty string if not found
                }
            }
            
            // Check for control keywords
            if (str_cmp(tokens[count].value, "if") == 0) tokens[count].type = TOKEN_IF;
            else if (str_cmp(tokens[count].value, "then") == 0) tokens[count].type = TOKEN_THEN;
            else if (str_cmp(tokens[count].value, "else") == 0) tokens[count].type = TOKEN_ELSE;
            else if (str_cmp(tokens[count].value, "fi") == 0) tokens[count].type = TOKEN_FI;
            else if (str_cmp(tokens[count].value, "while") == 0) tokens[count].type = TOKEN_WHILE;
            else if (str_cmp(tokens[count].value, "do") == 0) tokens[count].type = TOKEN_DO;
            else if (str_cmp(tokens[count].value, "done") == 0) tokens[count].type = TOKEN_DONE;
            
            count++;
        }
    }
    
    // Mark the end of the token stream
    tokens[count].type = TOKEN_EOF;
    return count;
}
