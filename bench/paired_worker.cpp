// One uninstrumented process measures one validator on one case.
#include "benchmark_solver.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
using benchmark::Rack;
using benchmark::Validator;
using Clock = std::chrono::steady_clock;
using Time = Clock::time_point;

double ms(Time start, Time end)
{
    return std::chrono::duration<double, std::milli>(end - start).count();
}

struct Solve {
    Time start;
    Time end;
    DirectionalCounts counts;
};

Solve measure_rack(const Validator &validator, Board &board, const Rack &rack)
{
    auto start = Clock::now();
    auto counts = validator.solve(board, rack);
    return {start, Clock::now(), counts};
}

struct BoardSolve {
    Time board_start;
    Time runs_start;
    Solve solve;
};

BoardSolve measure_board(const Validator &validator, Board &board, const char *path, const Rack &rack)
{
    auto start = Clock::now();
    board = board_from_csv(path);
    Time runs_start = validator.packed ? Clock::now() : Time{};
    validator.prepare(board);
    Solve solve = measure_rack(validator, board, rack);
    return {start, validator.packed ? runs_start : solve.start, solve};
}

struct DictionaryPhases {
    double read, load, build, save, construct;
};

void print_observation(const char *workload, bool hit, Time start, const BoardSolve &board,
                       DictionaryPhases dictionary = {})
{
    const auto &solve = board.solve;
    std::printf("%s,%s,%zu,%zu,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                workload, hit ? "hit" : "miss", solve.counts.across, solve.counts.down,
                ms(start, solve.end), dictionary.read, dictionary.load, dictionary.build,
                dictionary.save, dictionary.construct, ms(board.board_start, board.runs_start),
                ms(board.runs_start, solve.start), ms(solve.start, solve.end));
}

void measure(bool use_packed, bool expect_hit, const char *dictionary_path, const char *cache_path,
             const char *first_path, const Rack &first_rack, const char *next_path,
             const Rack &next_rack, const Rack &additional)
{
    Board first_board{};
    auto start = Clock::now();
    WordTable words = WordTable::from_file(dictionary_path);
    auto load_start = Clock::now();
    if (!words.count) {
        throw std::runtime_error("dictionary read failed");
    }
    WordPatternTable patterns = WordPatternTable::load(words, cache_path);
    bool hit = !patterns.is_empty();
    auto load_end = Clock::now();
    auto build_end = load_end;
    auto construct_start = load_end;
    if (hit != expect_hit) {
        throw std::runtime_error("pattern cache does not match expected starting condition");
    }
    if (!hit) {
        patterns = WordPatternTable::from_words(words);
        build_end = Clock::now();
        if (patterns.is_empty() || !patterns.save(words, cache_path)) {
            throw std::runtime_error("pattern cache build or save failed");
        }
        construct_start = Clock::now();
    }
    {
        Validator validator(words, patterns, use_packed);
        BoardSolve first = measure_board(validator, first_board, first_path, first_rack);
        first_board = {};
        Board next_board{};
        BoardSolve next = measure_board(validator, next_board, next_path, next_rack);
        Solve last = measure_rack(validator, next_board, additional);

        std::puts("workload,cache,across,down,total_ms,dictionary_ms,pattern_load_ms,pattern_build_ms,"
                  "pattern_save_ms,validator_ms,board_ms,run_starts_ms,solve_ms");
        print_observation("first_board", hit, start, first,
                          {ms(start, load_start), ms(load_start, load_end), ms(load_end, build_end),
                           ms(build_end, construct_start), ms(construct_start, first.board_start)});
        print_observation("new_board", hit, next.board_start, next);
        print_observation("new_rack", hit, last.start, {last.start, last.start, last});
    }
    words.destroy();
}
}

int main(int argc, char **argv)
{
    if (argc == 4 && std::strcmp(argv[1], "--check-cache") == 0) {
        return benchmark::check_cache(argv[2], argv[3]);
    }
    if (argc != 10 || (std::strcmp(argv[1], "original") && std::strcmp(argv[1], "packed")) ||
        (std::strcmp(argv[2], "hit") && std::strcmp(argv[2], "miss"))) {
        std::fprintf(stderr, "usage: %s original|packed hit|miss dictionary cache board rack new-board rack additional-rack\n", argv[0]);
        return 1;
    }
    try {
        measure(std::strcmp(argv[1], "packed") == 0, std::strcmp(argv[2], "hit") == 0, argv[3], argv[4],
                argv[5], Rack(argv[6]), argv[7], Rack(argv[8]), Rack(argv[9]));
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
