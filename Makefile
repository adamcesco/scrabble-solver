CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=build/%.o)
BOARD_FROM_CSV_TEST_SRC = tests/board_from_csv_test.c src/board.c
MAKE_ROW_TEST_SRC = tests/make_row_test.c src/board.c
ROW_TEST_SRC = tests/row_can_house_test.c src/board.c
DICTIONARY_TEST_SRC = tests/dictionary_from_file_test.c src/dictionary.c
TARGET = build/main
BOARD_FROM_CSV_TEST_TARGET = build/board_from_csv_test
MAKE_ROW_TEST_TARGET = build/make_row_test
ROW_TEST_TARGET = build/row_can_house_test
DICTIONARY_TEST_TARGET = build/dictionary_from_file_test
TEST_TARGETS = $(BOARD_FROM_CSV_TEST_TARGET) $(MAKE_ROW_TEST_TARGET) $(ROW_TEST_TARGET) $(DICTIONARY_TEST_TARGET)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

$(BOARD_FROM_CSV_TEST_TARGET): $(BOARD_FROM_CSV_TEST_SRC)
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^

$(MAKE_ROW_TEST_TARGET): $(MAKE_ROW_TEST_SRC)
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^

$(ROW_TEST_TARGET): $(ROW_TEST_SRC)
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^

$(DICTIONARY_TEST_TARGET): $(DICTIONARY_TEST_SRC)
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^

test: $(TEST_TARGETS)
	./$(BOARD_FROM_CSV_TEST_TARGET)
	./$(MAKE_ROW_TEST_TARGET)
	./$(ROW_TEST_TARGET)
	./$(DICTIONARY_TEST_TARGET)

clean:
	rm -rf build/

.PHONY: all test clean
