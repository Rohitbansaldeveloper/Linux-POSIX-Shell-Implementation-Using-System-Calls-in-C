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

// Helper to identify whitespace characters
static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Helper to identify special shell characters that should be their own tokens
static int is_special(char c) {
    return c == '|' || c == '<' || c == '>' || c == '&';
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
            tokens[count].type = TOKEN_PIPE;
            tokens[count].value[0] = '|';
            tokens[count].value[1] = '\0';
            input++;
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
            tokens[count].type = TOKEN_BACKGROUND;
            tokens[count].value[0] = '&';
            tokens[count].value[1] = '\0';
            input++;
            count++;
        } else {
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
            
            count++;
        }
    }
    
    // Mark the end of the token stream
    tokens[count].type = TOKEN_EOF;
    return count;
}
