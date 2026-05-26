/*
 * terminal.c
 *
 * Implements low-level terminal I/O (raw mode) to support features like
 * command history using arrow keys and basic keystroke interpretation.
 */

#include "terminal.h"
#include "syscalls.h"
#include "string_utils.h"

// termios constants for Linux x86_64
#define TCGETS 0x5401
#define TCSETS 0x5402
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

int read_line_raw(char *buffer, int max_len) {
    int pos = 0;
    int history_index = history_count;
    
    enable_raw_mode();
    
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
                sys_write(1, "\b \b", 3);
            }
        } else if (c == 27) { // Escape sequence
            char seq[2];
            if (sys_read(0, &seq[0], 1) != 1) continue;
            if (sys_read(0, &seq[1], 1) != 1) continue;
            
            if (seq[0] == '[') {
                if (seq[1] == 'A') { // Up arrow
                    if (history_index > 0) {
                        history_index--;
                        // Clear current line visually
                        while (pos > 0) { sys_write(1, "\b \b", 3); pos--; }
                        // Copy history into buffer
                        str_cpy(buffer, history[history_index]);
                        pos = str_len(buffer);
                        sys_write(1, buffer, pos); // Print it
                    }
                } else if (seq[1] == 'B') { // Down arrow
                    if (history_index < history_count - 1) {
                        history_index++;
                        while (pos > 0) { sys_write(1, "\b \b", 3); pos--; }
                        str_cpy(buffer, history[history_index]);
                        pos = str_len(buffer);
                        sys_write(1, buffer, pos);
                    } else if (history_index == history_count - 1) {
                        history_index++;
                        while (pos > 0) { sys_write(1, "\b \b", 3); pos--; }
                        buffer[0] = '\0'; // Back to empty prompt
                    }
                }
            }
        } else if (c == 4) { // Ctrl+D (EOF)
            if (pos == 0) {
                disable_raw_mode();
                return 0; // Signal EOF
            }
        } else if (c == 9) { // Tab key
            // Autocompletion stub (could match commands in PATH here)
        } else {
            // Normal character
            if (pos < max_len - 1) {
                buffer[pos++] = c;
                sys_write(1, &c, 1); // Echo manually
            }
        }
    }
    
    disable_raw_mode();
    if (pos > 0) {
        add_history(buffer);
    }
    return pos;
}
