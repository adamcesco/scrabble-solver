CXX = c++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Iinclude
SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:src/%.cpp=build/%.o)
BOARD_FROM_CSV_TEST_SRC = tests/board_from_csv_test.cpp src/board.cpp
CONFIG_MAPS_TEST_SRC = tests/config_maps_test.cpp src/board.cpp
MAKE_ROW_TEST_SRC = tests/make_row_test.cpp src/board.cpp
ROW_TEST_SRC = tests/row_can_house_test.cpp src/board.cpp
DICTIONARY_TEST_SRC = tests/dictionary_from_file_test.cpp src/dictionary.cpp src/board.cpp
WORD_START_ROW_TABLE_TEST_SRC = tests/word_start_row_table_test.cpp src/dictionary.cpp src/board.cpp
WORD_PATTERN_TABLE_TEST_SRC = tests/word_pattern_table_test.cpp src/dictionary.cpp src/board.cpp
VALIDATION_TEST_SRC = tests/validation_test.cpp src/validation.cpp src/dictionary.cpp src/board.cpp
TARGET = build/main
BOARD_FROM_CSV_TEST_TARGET = build/board_from_csv_test
CONFIG_MAPS_TEST_TARGET = build/config_maps_test
MAKE_ROW_TEST_TARGET = build/make_row_test
ROW_TEST_TARGET = build/row_can_house_test
DICTIONARY_TEST_TARGET = build/dictionary_from_file_test
WORD_START_ROW_TABLE_TEST_TARGET = build/word_start_row_table_test
WORD_PATTERN_TABLE_TEST_TARGET = build/word_pattern_table_test
VALIDATION_TEST_TARGET = build/validation_test
TEST_TARGETS = $(BOARD_FROM_CSV_TEST_TARGET) $(CONFIG_MAPS_TEST_TARGET) $(MAKE_ROW_TEST_TARGET) $(ROW_TEST_TARGET) $(DICTIONARY_TEST_TARGET) $(WORD_START_ROW_TABLE_TEST_TARGET) $(WORD_PATTERN_TABLE_TEST_TARGET) $(VALIDATION_TEST_TARGET)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BOARD_FROM_CSV_TEST_TARGET): $(BOARD_FROM_CSV_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $^

$(CONFIG_MAPS_TEST_TARGET): $(CONFIG_MAPS_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $^

$(MAKE_ROW_TEST_TARGET): $(MAKE_ROW_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $^

$(ROW_TEST_TARGET): $(ROW_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $^

$(DICTIONARY_TEST_TARGET): $(DICTIONARY_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $^

$(WORD_START_ROW_TABLE_TEST_TARGET): $(WORD_START_ROW_TABLE_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $^

$(WORD_PATTERN_TABLE_TEST_TARGET): $(WORD_PATTERN_TABLE_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $^

$(VALIDATION_TEST_TARGET): $(VALIDATION_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TEST_TARGETS)
	./$(BOARD_FROM_CSV_TEST_TARGET)
	./$(CONFIG_MAPS_TEST_TARGET)
	./$(MAKE_ROW_TEST_TARGET)
	./$(ROW_TEST_TARGET)
	./$(DICTIONARY_TEST_TARGET)
	./$(WORD_START_ROW_TABLE_TEST_TARGET)
	./$(WORD_PATTERN_TABLE_TEST_TARGET)
	./$(VALIDATION_TEST_TARGET)

clean:
	rm -rf build/

.PHONY: all test clean
