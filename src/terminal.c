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
#include "env.h"

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
static char history_file[256];

static void init_history_file(void) {
    char *home = get_env_val("HOME");
    if (home) {
        str_cpy(history_file, home);
        int len = str_len(history_file);
        str_cpy(history_file + len, "/.minishell_history");
    } else {
        history_file[0] = '\0';
    }
}

void load_history(void) {
    init_history_file();
    if (history_file[0] == '\0') return;
    
    int fd = sys_open(history_file, O_RDONLY, 0);
    if (fd < 0) return;
    
    char buf[4096];
    int n = sys_read(fd, buf, sizeof(buf));
    if (n > 0) {
        int line_start = 0;
        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                buf[i] = '\0';
                if (i - line_start > 0 && history_count < HISTORY_MAX) {
                    str_cpy(history[history_count++], &buf[line_start]);
                }
                line_start = i + 1;
            }
        }
    }
    sys_close(fd);
}

void save_history_line(const char *line) {
    if (history_file[0] == '\0') return;
    
    int fd = sys_open(history_file, O_WRONLY | O_CREAT | O_APPEND, 0644); 
    if (fd >= 0) {
        sys_write(fd, line, str_len(line));
        sys_write(fd, "\n", 1);
        sys_close(fd);
    }
}

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
    
    int epfd = sys_epoll_create1(0);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = 0;
    sys_epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &ev);
    struct epoll_event events[1];
    
    while (1) {
        int n = sys_epoll_wait(epfd, events, 1, -1);
        if (n <= 0) continue;
        
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
        } else if (c == 18) { // Ctrl+R
            char search_buf[128] = {0};
            int search_pos = 0;
            int match_index = -1;
            char search_prompt[256];
            
            while (1) {
                // Render search prompt
                sys_write(1, "\r\033[2K", 5);
                str_cpy(search_prompt, "(reverse-i-search)`");
                str_cpy(search_prompt + str_len(search_prompt), search_buf);
                str_cpy(search_prompt + str_len(search_prompt), "': ");
                sys_write(1, search_prompt, str_len(search_prompt));
                
                if (match_index >= 0) {
                    sys_write(1, history[match_index], str_len(history[match_index]));
                }
                
                char sc;
                if (sys_read(0, &sc, 1) != 1) break;
                
                if (sc == '\n' || sc == '\r') {
                    if (match_index >= 0) {
                        str_cpy(buffer, history[match_index]);
                        pos = str_len(buffer);
                    }
                    sys_write(1, "\n", 1);
                    buffer[pos] = '\0';
                    disable_raw_mode();
                    if (pos > 0) {
                        add_history(buffer);
                        save_history_line(buffer);
                    }
                    return pos;
                } else if (sc == 127 || sc == '\b') {
                    if (search_pos > 0) {
                        search_buf[--search_pos] = '\0';
                    }
                } else if (sc == 27 || sc == 3) { // Esc or Ctrl+C
                    break;
                } else if (sc >= 32 && sc <= 126 && search_pos < 127) {
                    search_buf[search_pos++] = sc;
                    search_buf[search_pos] = '\0';
                } else if (sc == 18) { // Ctrl+R again
                    if (match_index > 0) {
                        match_index--;
                    }
                }
                
                // Find match
                int found = -1;
                if (search_pos > 0) {
                    int start_search = (sc == 18 && match_index >= 0) ? match_index : history_count - 1;
                    for (int i = start_search; i >= 0; i--) {
                        if (str_str(history[i], search_buf) != NULL) {
                            found = i;
                            break;
                        }
                    }
                }
                match_index = found;
            }
            render_line(prompt, buffer, pos);
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
            
            // Check if it's the first word (a command)
            int is_cmd = 1;
            for (int i = 0; i < word_start; i++) {
                if (buffer[i] != ' ') {
                    is_cmd = 0;
                    break;
                }
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
                int found = 0;
                
                if (is_cmd) {
                    char *path_env = get_env_val("PATH");
                    if (path_env) {
                        char path_copy[1024];
                        str_cpy(path_copy, path_env);
                        char *dir = path_copy;
                        for (int i = 0; path_copy[i]; i++) {
                            if (path_copy[i] == ':') {
                                path_copy[i] = '\0';
                                if (autocomplete_match_in_dir(dir, prefix, match, 256)) {
                                    found = 1;
                                    break;
                                }
                                dir = &path_copy[i + 1];
                            }
                        }
                        if (!found && autocomplete_match_in_dir(dir, prefix, match, 256)) {
                            found = 1;
                        }
                    }
                    if (!found) { // fallback to current dir
                        found = autocomplete_match_in_dir(".", prefix, match, 256);
                    }
                } else {
                    found = autocomplete_match_in_dir(".", prefix, match, 256);
                }
                
                if (found) {
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
    
    sys_close(epfd);
    disable_raw_mode();
    if (pos > 0) {
        add_history(buffer);
        save_history_line(buffer);
    }
    return pos;
}
