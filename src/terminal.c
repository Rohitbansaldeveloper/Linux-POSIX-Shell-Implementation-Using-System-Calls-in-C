/*
 * terminal.c
 *
 * Implements low-level terminal I/O (raw mode) to support features like
 * command history using arrow keys and basic keystroke interpretation.
 */

#include "terminal.h"
#include "syscalls.h"
#include "string_utils.h"
#include "dirent.h"

// termios constants for Linux x86_64
#define TCGETS 0x5401
#define TCSETS 0x5402
#define TIOCGPGRP 0x540F
#define TIOCSPGRP 0x5410
#define ICANON 0000002
#define ECHO   0000010

struct termios {
    unsigned int c_iflag;
    unsigned int c_oflag;
    unsigned int c_cflag;
    unsigned int c_lflag;
    unsigned char c_line;
    unsigned char c_cc[32];
};

static struct termios orig_termios;
static int raw_mode_enabled = 0;

void enable_raw_mode(void) {
    if (raw_mode_enabled) return;
    sys_ioctl(0, TCGETS, &orig_termios);
    struct termios raw = orig_termios;
    // Disable canonical mode (line buffering) and echo
    raw.c_lflag &= ~(ICANON | ECHO); 
    sys_ioctl(0, TCSETS, &raw);
    raw_mode_enabled = 1;
}

void disable_raw_mode(void) {
    if (!raw_mode_enabled) return;
    sys_ioctl(0, TCSETS, &orig_termios);
    raw_mode_enabled = 0;
}

// History array
static char history[HISTORY_MAX][LINE_MAX_LEN];
static int history_count = 0;

void add_history(const char *line) {
    if (str_len(line) == 0) return;
    
    // Don't add duplicate if it's the exact same as the last command
    if (history_count > 0 && str_cmp(history[history_count - 1], line) == 0) return;
    
    if (history_count < HISTORY_MAX) {
        str_cpy(history[history_count], line);
        history_count++;
    } else {
        // Shift everything left to make room (inefficient but simple)
        for (int i = 1; i < HISTORY_MAX; i++) {
            str_cpy(history[i - 1], history[i]);
        }
        str_cpy(history[HISTORY_MAX - 1], line);
    }
}

int set_foreground_pgrp(int fd, int pgrp) {
    return sys_ioctl(fd, TIOCSPGRP, &pgrp);
}

int get_foreground_pgrp(int fd) {
    int pgrp;
    sys_ioctl(fd, TIOCGPGRP, &pgrp);
    return pgrp;
}

static void render_line(const char *prompt, const char *buffer, int pos) {
    // 1. Move cursor to column 0 and clear entire line
    sys_write(1, "\r\033[2K", 5);
    
    // 2. Print prompt
    sys_write(1, prompt, str_len(prompt));
    
    // 3. Tokenize dynamically and print with colors
    int in_quote = 0;
    int is_first_word = 1;
    
    for (int i = 0; i < pos; i++) {
        char c = buffer[i];
        if (c == '"') {
            in_quote = !in_quote;
            if (in_quote) {
                sys_write(1, "\033[33m\"", 6); // Yellow
            } else {
                sys_write(1, "\"\033[0m", 5); // Reset
            }
        } else if (in_quote) {
            sys_write(1, &c, 1);
        } else if (c == ' ') {
            sys_write(1, &c, 1);
            if (is_first_word && i > 0 && buffer[i-1] != ' ') {
                is_first_word = 0;
                sys_write(1, "\033[0m", 4);
            }
        } else if (c == '|' || c == '<' || c == '>' || c == '&' || c == ';') {
            sys_write(1, "\033[36m", 5); // Cyan
            sys_write(1, &c, 1);
            sys_write(1, "\033[0m", 4);
            is_first_word = 1; // Next word is a new command
        } else {
            if (is_first_word && (i == 0 || buffer[i-1] == ' ' || buffer[i-1] == '|' || buffer[i-1] == '&' || buffer[i-1] == ';')) {
                sys_write(1, "\033[32m", 5); // Green for commands
            }
            sys_write(1, &c, 1);
        }
    }
    sys_write(1, "\033[0m", 4); // Ensure reset
}

int read_line_raw(const char *prompt, char *buffer, int max_len) {
    int pos = 0;
    int history_index = history_count;
    
    enable_raw_mode();
    render_line(prompt, buffer, pos);
    
    while (1) {
        char c;
        if (sys_read(0, &c, 1) != 1) break;
        
        if (c == '\n' || c == '\r') {
            sys_write(1, "\n", 1);
            buffer[pos] = '\0';
            break;
        } else if (c == 127 || c == '\b') { // Backspace
            if (pos > 0) {
                pos--;
                buffer[pos] = '\0';
                render_line(prompt, buffer, pos);
            }
        } else if (c == 27) { // Escape sequence
            char seq[2];
            if (sys_read(0, &seq[0], 1) != 1) continue;
            if (sys_read(0, &seq[1], 1) != 1) continue;
            
            if (seq[0] == '[') {
                if (seq[1] == 'A') { // Up arrow
                    if (history_index > 0) {
                        history_index--;
                        str_cpy(buffer, history[history_index]);
                        pos = str_len(buffer);
                        render_line(prompt, buffer, pos);
                    }
                } else if (seq[1] == 'B') { // Down arrow
                    if (history_index < history_count - 1) {
                        history_index++;
                        str_cpy(buffer, history[history_index]);
                        pos = str_len(buffer);
                        render_line(prompt, buffer, pos);
                    } else if (history_index == history_count - 1) {
                        history_index++;
                        buffer[0] = '\0'; // Back to empty prompt
                        pos = 0;
                        render_line(prompt, buffer, pos);
                    }
                }
            }
        } else if (c == 4) { // Ctrl+D (EOF)
            if (pos == 0) {
                disable_raw_mode();
                return 0; // Signal EOF
            }
        } else if (c == 9) { // Tab key
            // Autocompletion via raw directory reading
            // 1. Find the start of the current word
            int word_start = pos;
            while (word_start > 0 && buffer[word_start - 1] != ' ') {
                word_start--;
            }
            
            // 2. Extract the prefix
            char prefix[256];
            int prefix_len = pos - word_start;
            if (prefix_len > 0 && prefix_len < 255) {
                for (int i = 0; i < prefix_len; i++) {
                    prefix[i] = buffer[word_start + i];
                }
                prefix[prefix_len] = '\0';
                
                // 3. Search directory
                char match[256];
                if (autocomplete_match(prefix, match, 256)) {
                    // Append the remainder of the match to the buffer
                    int match_len = str_len(match);
                    for (int i = prefix_len; i < match_len && pos < max_len - 1; i++) {
                        buffer[pos++] = match[i];
                    }
                    buffer[pos] = '\0';
                    render_line(prompt, buffer, pos);
                }
            }
        } else {
            // Normal character
            if (pos < max_len - 1) {
                buffer[pos++] = c;
                buffer[pos] = '\0';
                render_line(prompt, buffer, pos);
            }
        }
    }
    
    disable_raw_mode();
    if (pos > 0) {
        add_history(buffer);
    }
    return pos;
}
