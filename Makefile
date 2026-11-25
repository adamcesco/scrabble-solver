CXX = c++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Iinclude
SRC = $(wildcard src/*.cpp src/solvers/*/*.cpp)
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
SOLVER_SRC = $(wildcard src/solvers/*/*.cpp) src/perpendicular_checks.cpp $(DICTIONARY_SRC) src/board.cpp src/utils.cpp
SOLVER_HEADERS = $(wildcard include/*.h include/solvers/*.h src/solvers/*/*.h)
RACK_ORIENTED_SOLVER_TEST_SRC = tests/rack_oriented_solver_test.cpp $(SOLVER_SRC)
WINDOW_REGISTRY = bench/window_counters/window_variants.cpp
COMPARISON_SRC = tests/placement_comparison.cpp $(WINDOW_REGISTRY) $(SOLVER_SRC)
WINDOW_HEADERS = $(wildcard bench/window_counters/variants/*.h) bench/window_counters/window_variants.h bench/measurement_support/benchmark_solver.h
WINDOW_FLAGS = -Isrc -Ibench/window_counters -Ibench/measurement_support
# Keep a run's qualifier, worker and embedded build details together across concurrent configurations.
WINDOW_CONFIG := $(shell printf '%s\n' '$(CXX) $(CXXFLAGS) $(WINDOW_FLAGS)' | git hash-object --stdin)
WINDOW_BUILD = build/window/$(WINDOW_CONFIG)
WINDOW_TOOLS = $(addprefix $(WINDOW_BUILD)/,window_worker placement_equivalence_test perpendicular_checks_test)
COMPARISON_HEADERS = $(WINDOW_HEADERS) tests/placement_comparison.h tests/placement_trace.h tests/perpendicular_fixtures.h tests/square_equivalence.h $(SOLVER_HEADERS)
PERPENDICULAR_CHECKS_TEST_SRC = tests/perpendicular_checks_test.cpp $(COMPARISON_SRC)
PLACEMENT_EQUIVALENCE_TEST_SRC = tests/placement_equivalence_test.cpp $(COMPARISON_SRC)
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
PLACEMENT_EQUIVALENCE_TEST_TARGET = build/placement_equivalence_test
TEST_TARGETS = $(BOARD_FROM_CSV_TEST_TARGET) $(CONFIG_MAPS_TEST_TARGET) $(MAKE_ROW_TEST_TARGET) $(ROW_TEST_TARGET) $(ROW_WINDOWS_TEST_TARGET) $(DICTIONARY_TEST_TARGET) $(WORD_START_ROW_TABLE_TEST_TARGET) $(WORD_PATTERN_TABLE_TEST_TARGET) $(VALIDATION_HOT_TEST_TARGET) $(RACK_ORIENTED_SOLVER_TEST_TARGET)
TEST_TARGETS += $(PERPENDICULAR_CHECKS_TEST_TARGET) $(PLACEMENT_EQUIVALENCE_TEST_TARGET)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJ:.o=.d)

$(RACK_ORIENTED_SOLVER_TEST_TARGET): $(SOLVER_HEADERS)
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

$(PERPENDICULAR_CHECKS_TEST_TARGET) $(WINDOW_BUILD)/perpendicular_checks_test: $(PERPENDICULAR_CHECKS_TEST_SRC) $(COMPARISON_HEADERS)
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(WINDOW_FLAGS) -UNDEBUG -DSCRABBLE_TRACE_PLACEMENTS -Itests -o $@.$$$$ $(PERPENDICULAR_CHECKS_TEST_SRC) && mv $@.$$$$ $@

$(RACK_ORIENTED_SOLVER_TEST_TARGET): $(RACK_ORIENTED_SOLVER_TEST_SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $(RACK_ORIENTED_SOLVER_TEST_SRC)

$(PLACEMENT_EQUIVALENCE_TEST_TARGET) $(WINDOW_BUILD)/placement_equivalence_test: $(PLACEMENT_EQUIVALENCE_TEST_SRC) $(COMPARISON_HEADERS)
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(WINDOW_FLAGS) -UNDEBUG -DSCRABBLE_TRACE_PLACEMENTS -Itests -o $@.$$$$ $(PLACEMENT_EQUIVALENCE_TEST_SRC) && mv $@.$$$$ $@

build/paired_worker: bench/validator_comparison/paired_worker.cpp bench/measurement_support/benchmark_solver.h $(SOLVER_SRC) $(SOLVER_HEADERS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -Ibench/measurement_support -o $@ bench/validator_comparison/paired_worker.cpp $(SOLVER_SRC)

$(WINDOW_BUILD)/window_worker: bench/window_counters/window_worker.cpp $(WINDOW_REGISTRY) $(WINDOW_HEADERS) $(SOLVER_SRC) $(SOLVER_HEADERS)
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(WINDOW_FLAGS) -DWINDOW_BUILD_FLAGS='"$(CXXFLAGS) $(WINDOW_FLAGS)"' -o $@.$$$$ bench/window_counters/window_worker.cpp $(WINDOW_REGISTRY) $(SOLVER_SRC) && mv $@.$$$$ $@

window-tools: $(WINDOW_TOOLS)
	@echo $(WINDOW_BUILD)

window-benchmark: $(WINDOW_TOOLS)
	python3 -m bench.window_counters.window_benchmark --build-dir $(WINDOW_BUILD) $(BENCH_ARGS)

# Allocation entry points used by the solver on x86-64 Linux with libstdc++.
MEMORY_WRAP = $(foreach name,malloc calloc free _Znwm _ZdlPv _ZdlPvm,-Wl,--wrap=$(name))
build/memory_worker: bench/memory/memory_worker.cpp bench/memory/memory_trace.cpp bench/memory/memory_trace.h bench/measurement_support/benchmark_solver.h $(SOLVER_SRC) $(SOLVER_HEADERS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -DSCRABBLE_MEMORY_TRACE -Ibench/memory -Ibench/measurement_support -o $@ bench/memory/memory_worker.cpp bench/memory/memory_trace.cpp $(SOLVER_SRC) $(MEMORY_WRAP)

memory-benchmark: build/memory_worker
	python3 -m bench.memory.memory_benchmark $(BENCH_ARGS)

paired-benchmark: build/paired_worker $(PERPENDICULAR_CHECKS_TEST_TARGET) $(PLACEMENT_EQUIVALENCE_TEST_TARGET)
	python3 -m bench.validator_comparison.paired_benchmark $(BENCH_ARGS)

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

check-bench-imports:
	python3 tests/bench_imports_test.py

check-solver-deps:
	python3 tests/solver_dependencies_test.py $(CXX) $(CXXFLAGS)

test: check-solver-deps check-bench-imports $(TEST_TARGETS) build/paired_worker $(WINDOW_TOOLS)
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
	WINDOW_BUILD=$(abspath $(WINDOW_BUILD)) python3 tests/window_benchmark_test.py
	WINDOW_BUILD=$(abspath $(WINDOW_BUILD)) python3 tests/paired_benchmark_test.py
	python3 tests/mass_comparison_test.py
	python3 tests/replay_game_test.py
	python3 tests/sample_turns_test.py

clean:
	rm -rf build/

.PHONY: all test check-solver-deps check-bench-imports test-memory paired-benchmark memory-benchmark window-benchmark window-tools clean
