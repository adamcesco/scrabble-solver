CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=build/%.o)
	TARGET = build/main

all: $(TARGET)

$(TARGET): $(OBJ)
		$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c
		mkdir -p build
			$(CC) $(CFLAGS) -c $< -o $@

clean:
		rm -rf build/

.PHONY: all clean

