#ifndef TERMINAL_H
#define TERMINAL_H

#include "syscalls.h"

// History settings
#define HISTORY_MAX 50
#define LINE_MAX_LEN 1024

void enable_raw_mode(void);
void disable_raw_mode(void);

// Reads a line from the terminal using raw mode, supporting arrow keys and history.
int read_line_raw(char *buffer, int max_len);

// Add a line to history
void add_history(const char *line);

// Terminal process group control
int set_foreground_pgrp(int fd, int pgrp);
int get_foreground_pgrp(int fd);

#endif
