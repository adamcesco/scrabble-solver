CXX = c++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Iinclude
CORE = $(filter-out src/main.cpp,$(wildcard src/*.cpp))
OBJECTS = $(CORE:src/%.cpp=build/%.o)
TESTS = board_from_csv make_row dictionary_from_file row_windows word_pattern_table perpendicular_checks solver
TEST_BINS = $(addprefix build/,$(addsuffix _test,$(TESTS))) build/candidate_decisions_test
TRACE_FLAGS = -Itests -Ibench/measurement_support -DSCRABBLE_TRACE_PLACEMENTS -UNDEBUG
TRACE_OBJECTS = $(filter-out build/solver.o,$(OBJECTS)) build/trace/solver.o build/trace/placement_trace.o

all: build/main

build/main: build/main.o $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

build/tests/%.o: tests/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -UNDEBUG -MMD -MP -c $< -o $@

build/%_test: build/tests/%_test.o $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/trace/solver.o: src/solver.cpp
build/trace/placement_trace.o: tests/placement_trace.cpp
build/trace/candidate_decisions_test.o: tests/candidate_decisions_test.cpp
build/trace/qualification.o: bench/selective_anagram/qualification.cpp

build/trace/%.o:
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(TRACE_FLAGS) -MMD -MP -c $< -o $@

build/candidate_decisions_test: build/trace/candidate_decisions_test.o $(TRACE_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/qualification_worker: build/trace/qualification.o $(TRACE_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

.SECONDARY:
-include $(wildcard build/*.d build/tests/*.d build/trace/*.d)

test: build/main $(TEST_BINS) build/qualification_worker
	@set -e; for test in $(TEST_BINS); do ./$$test; done
	python3 tests/cli_test.py
	python3 -m bench.selective_anagram.qualification_test
	python3 tests/replay_game_test.py
	python3 tests/sample_turns_test.py

clean:
	rm -rf build/

.PHONY: all test clean
