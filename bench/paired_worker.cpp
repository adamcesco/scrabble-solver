// One uninstrumented process measures one validator on one case.
#include "rack_oriented_solver.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {
using Clock = std::chrono::steady_clock;
using Time = Clock::time_point;

double ms(Time start, Time end)
{
    return std::chrono::duration<double, std::milli>(end - start).count();
}

struct Rack {
    uint64_t tiles = 0;
    uint8_t length;

    explicit Rack(std::string_view letters)
    {
        if (letters.empty() || letters.size() > MAX_RACK_TILES ||
            !std::is_sorted(letters.begin(), letters.end()) ||
            letters.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") != letters.npos) {
            throw std::invalid_argument("expected 1-7 sorted A-Z rack tiles");
        }
        length = letters.size();
        std::memcpy(&tiles, letters.data(), length);
    }
};

struct Validator {
    const WordTable &words;
    const WordPatternTable &patterns;
    std::optional<PerpendicularCheckTable> packed;
    WordIndexStartRowTable original{};

    Validator(const WordTable &words, const WordPatternTable &patterns, bool use_packed)
        : words(words), patterns(patterns)
    {
        if (use_packed) {
            packed.emplace(words);
        } else {
            original = WordIndexStartRowTable::from_words(words);
            if (original.count != words.count) {
                throw std::runtime_error("original validator construction failed");
            }
        }
    }

    ~Validator() { original.destroy(); }

    DirectionalCounts solve(Board &board, RackSubsetKeys &keys, uint8_t length) const
    {
        if (packed) {
            return rack_oriented_both_directions(&words, &patterns, &keys, &board, length);
        }
        auto count = [&] {
            return rack_oriented_full_board_solver(&words, &patterns, &original, &keys, &board, length);
        };
        size_t across = count();
        board.swap_views();
        size_t down = count();
        board.swap_views();
        return {across, down};
    }
};

struct Solve {
    Time start;
    Time end;
    DirectionalCounts counts;
};

Solve measure_rack(const Validator &validator, Board &board, const Rack &rack)
{
    auto start = Clock::now();
    auto keys = rack_subset_keys_for_rack(rack.tiles, rack.length);
    auto counts = validator.solve(board, keys, rack.length);
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
    Time runs_start;
    if (validator.packed) {
        runs_start = Clock::now();
        board.prepare_perpendicular_checks(*validator.packed);
    }
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

void measure(bool use_packed, const char *dictionary_path, const char *cache_path,
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
    if (argc != 9 || (std::strcmp(argv[1], "original") && std::strcmp(argv[1], "packed"))) {
        std::fprintf(stderr, "usage: %s original|packed dictionary cache board rack new-board rack additional-rack\n", argv[0]);
        return 1;
    }
    try {
        measure(std::strcmp(argv[1], "packed") == 0, argv[2], argv[3],
                argv[4], Rack(argv[5]), argv[6], Rack(argv[7]), Rack(argv[8]));
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
