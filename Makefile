CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=build/%.o)
TEST_SRC = tests/board_from_csv_test.c src/board.c
TARGET = build/main
TEST_TARGET = build/board_from_csv_test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_TARGET): $(TEST_SRC)
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf build/

.PHONY: all test clean
