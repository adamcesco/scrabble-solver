CXX = c++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Iinclude
SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:src/%.cpp=build/%.o)
DICTIONARY_SRC = src/word_table.cpp src/word_start_row_table.cpp src/word_index_start_row_table.cpp src/word_pattern_table.cpp
BOARD_FROM_CSV_TEST_SRC = tests/board_from_csv_test.cpp src/board.cpp src/utils.cpp
CONFIG_MAPS_TEST_SRC = tests/config_maps_test.cpp src/board.cpp src/utils.cpp
MAKE_ROW_TEST_SRC = tests/make_row_test.cpp src/board.cpp src/utils.cpp
ROW_TEST_SRC = tests/row_can_house_test.cpp src/board.cpp src/utils.cpp
ROW_WINDOWS_TEST_SRC = tests/row_windows_test.cpp src/board.cpp src/utils.cpp
DICTIONARY_TEST_SRC = tests/dictionary_from_file_test.cpp $(DICTIONARY_SRC) src/board.cpp src/utils.cpp
WORD_START_ROW_TABLE_TEST_SRC = tests/word_start_row_table_test.cpp $(DICTIONARY_SRC) src/board.cpp src/utils.cpp
WORD_PATTERN_TABLE_TEST_SRC = tests/word_pattern_table_test.cpp $(DICTIONARY_SRC) src/board.cpp src/utils.cpp
VALIDATION_HOT_TEST_SRC = tests/validation_hot_test.cpp $(DICTIONARY_SRC) src/board.cpp src/utils.cpp
SOLVER_SRC = src/rack_oriented_solver.cpp src/perpendicular_checks.cpp $(DICTIONARY_SRC) src/board.cpp src/utils.cpp
SOLVER_HEADERS = include/rack_oriented_solver.h include/row_windows.h include/validation_hot.h include/perpendicular_checks.h \
                include/board.h include/utils.h include/dictionary.h $(DICTIONARY_SRC:src/%.cpp=include/%.h)
RACK_ORIENTED_SOLVER_TEST_SRC = tests/rack_oriented_solver_test.cpp $(SOLVER_SRC)
COMPARISON_SRC = tests/placement_comparison.cpp $(SOLVER_SRC)
COMPARISON_HEADERS = tests/placement_comparison.h tests/placement_trace.h tests/perpendicular_fixtures.h tests/square_equivalence.h $(SOLVER_HEADERS)
PERPENDICULAR_CHECKS_TEST_SRC = tests/perpendicular_checks_test.cpp $(COMPARISON_SRC)
PLACEMENT_EQUIVALENCE_TEST_SRC = tests/placement_equivalence_test.cpp $(COMPARISON_SRC)
SOLVER_BENCHMARK_SRC = bench/solver_benchmark.cpp $(SOLVER_SRC)
TARGET = build/main
BOARD_FROM_CSV_TEST_TARGET = build/board_from_csv_test
CONFIG_MAPS_TEST_TARGET = build/config_maps_test
MAKE_ROW_TEST_TARGET = build/make_row_test
ROW_TEST_TARGET = build/row_can_house_test
ROW_WINDOWS_TEST_TARGET = build/row_windows_test
DICTIONARY_TEST_TARGET = build/dictionary_from_file_test
WORD_START_ROW_TABLE_TEST_TARGET = build/word_start_row_table_test
WORD_PATTERN_TABLE_TEST_TARGET = build/word_pattern_table_test
VALIDATION_HOT_TEST_TARGET = build/validation_hot_test
PERPENDICULAR_CHECKS_TEST_TARGET = build/perpendicular_checks_test
RACK_ORIENTED_SOLVER_TEST_TARGET = build/rack_oriented_solver_test
SOLVER_BENCHMARK_TARGET = build/solver_benchmark
PLACEMENT_EQUIVALENCE_TEST_TARGET = build/placement_equivalence_test
TEST_TARGETS = $(BOARD_FROM_CSV_TEST_TARGET) $(CONFIG_MAPS_TEST_TARGET) $(MAKE_ROW_TEST_TARGET) $(ROW_TEST_TARGET) $(ROW_WINDOWS_TEST_TARGET) $(DICTIONARY_TEST_TARGET) $(WORD_START_ROW_TABLE_TEST_TARGET) $(WORD_PATTERN_TABLE_TEST_TARGET) $(VALIDATION_HOT_TEST_TARGET) $(RACK_ORIENTED_SOLVER_TEST_TARGET)
TEST_TARGETS += $(PERPENDICULAR_CHECKS_TEST_TARGET) $(PLACEMENT_EQUIVALENCE_TEST_TARGET)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/main.o build/rack_oriented_solver.o $(RACK_ORIENTED_SOLVER_TEST_TARGET) $(PLACEMENT_EQUIVALENCE_TEST_TARGET) $(SOLVER_BENCHMARK_TARGET): $(SOLVER_HEADERS)
build/board.o: include/board.h

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

$(ROW_WINDOWS_TEST_TARGET): $(ROW_WINDOWS_TEST_SRC) include/row_windows.h
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $(ROW_WINDOWS_TEST_SRC)

$(DICTIONARY_TEST_TARGET): $(DICTIONARY_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $^

$(WORD_START_ROW_TABLE_TEST_TARGET): $(WORD_START_ROW_TABLE_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $^

$(WORD_PATTERN_TABLE_TEST_TARGET): $(WORD_PATTERN_TABLE_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $^

$(VALIDATION_HOT_TEST_TARGET): $(VALIDATION_HOT_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $^

build/perpendicular_checks.o: include/perpendicular_checks.h include/validation_hot.h include/board.h include/word_table.h

$(PERPENDICULAR_CHECKS_TEST_TARGET): $(PERPENDICULAR_CHECKS_TEST_SRC) $(COMPARISON_HEADERS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -UNDEBUG -DSCRABBLE_TRACE_PLACEMENTS -Itests -o $@ $(PERPENDICULAR_CHECKS_TEST_SRC)

$(RACK_ORIENTED_SOLVER_TEST_TARGET): $(RACK_ORIENTED_SOLVER_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $(RACK_ORIENTED_SOLVER_TEST_SRC)

$(PLACEMENT_EQUIVALENCE_TEST_TARGET): $(PLACEMENT_EQUIVALENCE_TEST_SRC) $(COMPARISON_HEADERS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -UNDEBUG -DSCRABBLE_TRACE_PLACEMENTS -Itests -o $@ $(PLACEMENT_EQUIVALENCE_TEST_SRC)

$(SOLVER_BENCHMARK_TARGET): $(SOLVER_BENCHMARK_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $(SOLVER_BENCHMARK_SRC)

build/paired_worker: bench/paired_worker.cpp bench/benchmark_solver.h $(SOLVER_SRC) $(SOLVER_HEADERS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ bench/paired_worker.cpp $(SOLVER_SRC)

# Allocation entry points used by the solver on x86-64 Linux with libstdc++.
MEMORY_WRAP = $(foreach name,malloc calloc free _Znwm _ZdlPv _ZdlPvm,-Wl,--wrap=$(name))
build/memory_worker: bench/memory_worker.cpp bench/memory_trace.cpp bench/memory_trace.h bench/benchmark_solver.h $(SOLVER_SRC) $(SOLVER_HEADERS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -DSCRABBLE_MEMORY_TRACE -Ibench -o $@ bench/memory_worker.cpp bench/memory_trace.cpp $(SOLVER_SRC) $(MEMORY_WRAP)

memory-benchmark: build/memory_worker
	python3 bench/memory_benchmark.py $(BENCH_ARGS)

paired-benchmark: build/paired_worker $(PERPENDICULAR_CHECKS_TEST_TARGET) $(PLACEMENT_EQUIVALENCE_TEST_TARGET)
	python3 bench/paired_benchmark.py $(BENCH_ARGS)

test-memory: build/memory_worker
	python3 tests/memory_benchmark_test.py

# Keep the regular solver suite usable with other standard libraries/linkers.
ifeq ($(shell uname -sm),Linux x86_64)
ifneq ($(filter test,$(MAKECMDGOALS)),)
memory_toolchain := $(shell echo 'int main() { return __GLIBCXX__ == 0; }' | $(CXX) $(CXXFLAGS) -include cstddef -x c++ - -Wl,--wrap=calloc -o /dev/null 2>/dev/null && echo supported)
ifeq ($(memory_toolchain),supported)
test: test-memory
endif
endif
endif

test: $(TEST_TARGETS) build/paired_worker
	./$(BOARD_FROM_CSV_TEST_TARGET)
	./$(CONFIG_MAPS_TEST_TARGET)
	./$(MAKE_ROW_TEST_TARGET)
	./$(ROW_TEST_TARGET)
	./$(ROW_WINDOWS_TEST_TARGET)
	./$(DICTIONARY_TEST_TARGET)
	./$(WORD_START_ROW_TABLE_TEST_TARGET)
	./$(WORD_PATTERN_TABLE_TEST_TARGET)
	./$(VALIDATION_HOT_TEST_TARGET)
	./$(PERPENDICULAR_CHECKS_TEST_TARGET)
	./$(RACK_ORIENTED_SOLVER_TEST_TARGET)
	./$(PLACEMENT_EQUIVALENCE_TEST_TARGET)
	python3 tests/paired_benchmark_test.py
	python3 tests/mass_comparison_test.py
	python3 tests/replay_game_test.py
	python3 tests/sample_turns_test.py

benchmark: $(SOLVER_BENCHMARK_TARGET)
	./$(SOLVER_BENCHMARK_TARGET) data/boards/sample_board.csv data/dictionaries/sample_dictionary.txt 5000

clean:
	rm -rf build/

.PHONY: all test test-memory benchmark paired-benchmark memory-benchmark clean
