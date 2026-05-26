CC = gcc
CFLAGS = -Wall -Wextra -nostdlib -fno-builtin -static -Os -I./src
SRCS = src/main.c src/syscalls.c src/string_utils.c src/memory.c \
       src/dirent.c src/terminal.c src/env.c src/tokenizer.c src/parser.c \
       src/executor.c src/builtins.c src/signals.c src/jobs.c src/alias.c src/logger.c
OBJS = $(SRCS:.c=.o)
TARGET = minishell

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
