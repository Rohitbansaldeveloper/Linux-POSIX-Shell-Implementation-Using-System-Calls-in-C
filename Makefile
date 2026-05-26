CC = gcc
CFLAGS = -Wall -Wextra -nostdlib -fno-builtin -static -Os -I./src
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = minishell

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
