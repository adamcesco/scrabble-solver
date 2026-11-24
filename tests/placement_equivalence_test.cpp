#include "placement_trace.h"
#include "perpendicular_fixtures.h"
#include "rack_oriented_solver.h"
#include "square_equivalence.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using Decision = std::tuple<bool, int, int, std::string, bool>; // down, row, start, word, accepted
using Decisions = std::vector<Decision>;
using WindowDecisions = std::vector<std::pair<std::string, bool>>;
using LookupCounts = std::array<std::array<unsigned, BOARD_SIZE>, BOARD_SIZE>;

static void check_rows(const Row (&expected)[BOARD_SIZE], const Row (&actual)[BOARD_SIZE])
{
    for (int i = 0; i < BOARD_SIZE; ++i) {
        assert(actual[i].tiles == expected[i].tiles);
        assert(actual[i].occupiedMask == expected[i].occupiedMask);
        assert(actual[i].careMask == expected[i].careMask);
    }
}

struct SolverBoardSnapshot {
    Board original;

    void check(const Board &board, bool down) const
    {
        check_rows(down ? original.perpendicularRows : original.rows, board.rows);
        check_rows(down ? original.rows : original.perpendicularRows, board.perpendicularRows);
        // These pointers identify immutable grids, including IDs that no candidate reads.
        assert(board.normal_runs == (down ? original.perpendicular_runs : original.normal_runs));
        assert(board.perpendicular_runs == (down ? original.normal_runs : original.perpendicular_runs));
    }
};

struct Trace {
    const WordTable &dictionary;
    const Board &board;
    const SolverBoardSnapshot &snapshot;
    bool down;
    Decisions decisions;
    LookupCounts lookups{};
};

static Trace *active_trace;

void trace_square_lookup(uint8_t row, uint8_t col)
{
    if (active_trace) {
        ++active_trace->lookups[row][col];
    }
}

void trace_placements(uint8_t row, uint8_t start, const uint32_t *words, size_t count, bool accepted)
{
    if (active_trace) {
        active_trace->snapshot.check(active_trace->board, active_trace->down);
        for (size_t i = 0; i < count; ++i) {
            active_trace->decisions.emplace_back(active_trace->down, row, start,
                                                  active_trace->dictionary.words[words[i]], accepted);
        }
    }
}

static LookupCounts expected_lookups(const Board &board, const Decisions &decisions)
{
    std::set<std::tuple<int, int, size_t>> windows;
    for (const auto &[down, row, start, word, accepted] : decisions) {
        windows.emplace(row, start, word.size());
    }
    LookupCounts counts{};
    for (const auto &[row, start, length] : windows) {
        for (size_t col = start; col < start + length; ++col) {
            if (!(board.rows[row].occupiedMask & (1u << col))) {
                ++counts[row][col];
            }
        }
    }
    return counts;
}

static size_t accepted_count(const Decisions &decisions)
{
    return std::count_if(decisions.begin(), decisions.end(), [](const Decision &decision) {
        const auto &[down, row, start, word, accepted] = decision;
        return accepted;
    });
}

struct Fixture {
    const WordTable &words;
    WordPatternTable patterns = WordPatternTable::from_words(words);
    WordIndexStartRowTable rows = WordIndexStartRowTable::from_words(words);
    PerpendicularCheckTable lookup{words};

    ~Fixture() { rows.destroy(); }

    Decisions compare_pass(Board &board, const SolverBoardSnapshot &snapshot, bool down,
                           RackSubsetKeys &rack, uint8_t length) const
    {
        Trace reference{words, board, snapshot, down, {}};
        active_trace = &reference;
        size_t expected_count = rack_oriented_full_board_solver(
            &words, &patterns, &rows, &rack, &board, length);
        snapshot.check(board, down);

        Trace packed{words, board, snapshot, down, {}};
        active_trace = &packed;
        size_t count = rack_oriented_full_board_solver(&words, &patterns, &rack, &board, length);
        active_trace = nullptr;
        snapshot.check(board, down);

        // Compare sorted copies so duplicate decisions and the actual visit order are both observable.
        Decisions sorted = packed.decisions;
        std::sort(sorted.begin(), sorted.end());
        std::sort(reference.decisions.begin(), reference.decisions.end());
        assert(sorted == reference.decisions);
        assert(packed.lookups == expected_lookups(board, reference.decisions));
        assert(count == expected_count);
        assert(count == accepted_count(sorted));
        return packed.decisions;
    }
};

static WindowDecisions middle_window(const Decisions &decisions)
{
    WindowDecisions result;
    for (const auto &[down, row, start, word, accepted] : decisions) {
        if (row == 1 && start == 0 && word.size() == 3) {
            result.emplace_back(word, accepted);
        }
    }
    return result;
}

static RackSubsetKeys rack_for(const char *letters)
{
    uint64_t packed = 0;
    memcpy(&packed, letters, strlen(letters));
    return rack_subset_keys_for_rack(packed, strlen(letters));
}

static void compare_rt_orders(bool tar_first)
{
    WordTable words = load_dictionary(tar_first ? "AS\nSAT\nTO\nAR\nAT\nTAR\nRAT\n"
                                          : "AS\nSAT\nTO\nAR\nAT\nRAT\nTAR\n");
    {
        Fixture fixture{words};
        for (bool transposed : {false, true}) {
            Board board = board_at(0, 0, {"AS.", ".A.", ".TO"});
            if (transposed) {
                board.swap_views();
            }
            board.prepare_perpendicular_checks(fixture.lookup);
            SolverBoardSnapshot snapshot{board};
            RackSubsetKeys rt = rack_for("RT");

            DirectionalCounts counts = rack_oriented_both_directions(
                &words, &fixture.patterns, &rt, &board, 2);
            snapshot.check(board, false);
            for (int pass = 0; pass < 3; ++pass) {
                if (pass > 0) {
                    board.swap_views();
                }
                bool down = pass == 1;
                Decisions decisions = fixture.compare_pass(board, snapshot, down, rt, 2);
                assert((down ? counts.down : counts.across) == accepted_count(decisions));
                if (down == transposed) {
                    const uint32_t r = 1u << ('R' - 'A'), t = 1u << ('T' - 'A');
                    assert((board.perpendicular_check_set(1, 0) & (r | t)) == (r | t));
                    assert((board.perpendicular_check_set(1, 2) & (r | t)) == t);
                    WindowDecisions expected{{"TAR", false}, {"RAT", true}};
                    if (!tar_first) {
                        std::reverse(expected.begin(), expected.end());
                    }
                    assert(middle_window(decisions) == expected);
                }
            }
            snapshot.check(board, false);
            RackSubsetKeys zz = rack_for("ZZ");
            assert(fixture.compare_pass(board, snapshot, false, zz, 2).empty());
        }
    }
    words.destroy();
}

static void compare_rack_groups_and_ruled_out_windows()
{
    WordTable words = load_dictionary("AS\nASX\nSAT\nTO\nAR\nAT\nTAR\nRAT\n");
    {
        Fixture fixture{words};
        RackSubsetKeys rst = rack_for("RST");
        for (bool blocked : {false, true}) {
            Board board = board_at(0, 0, {blocked ? "ASX" : "AS.", ".A.", ".TO"});
            board.prepare_perpendicular_checks(fixture.lookup);
            SolverBoardSnapshot snapshot{board};
            Decisions across = fixture.compare_pass(board, snapshot, false, rst, 3);
            // SAT uses ST; TAR and RAT use RT. The X.O crossing disallows all three.
            WindowDecisions actual = middle_window(across);
            std::sort(actual.begin(), actual.end());
            const WindowDecisions expected{{"RAT", !blocked}, {"SAT", !blocked}, {"TAR", false}};
            assert(actual == expected);
            board.swap_views();
            fixture.compare_pass(board, snapshot, true, rst, 3);
            board.swap_views();
            snapshot.check(board, false);
        }
    }
    words.destroy();
}

static void one_tile_counts_in_both_directions_and_restores_the_prepared_board()
{
    WordTable words = load_dictionary("AS\nST\n");
    {
        Fixture fixture{words};
        Board board = board_at(0, 0, {"AS", "S."});
        board.prepare_perpendicular_checks(fixture.lookup);
        SolverBoardSnapshot snapshot{board};
        auto rack = rack_for("T");
        const auto counts = rack_oriented_both_directions(&words, &fixture.patterns, &rack, &board, 1);
        assert(counts.across == 1 && counts.down == 1);
        snapshot.check(board, false);
    }
    words.destroy();
}

// The benchmark sends one tab-separated board path and sorted rack per line.
static void qualify_inputs(const char *dictionary_path, const char *cache_path)
{
    WordTable words = WordTable::from_file(dictionary_path);
    assert(words.count > 0);
    {
        Fixture fixture{words};
        assert(!fixture.patterns.is_empty() && fixture.rows.count == words.count);
        if (!fixture.patterns.save(words, cache_path)) {
            throw std::runtime_error("could not save the shared pattern cache");
        }

        std::string line;
        while (std::getline(std::cin, line)) {
            size_t tab = line.find('\t');
            assert(tab != std::string::npos);
            std::string path = line.substr(0, tab), letters = line.substr(tab + 1);
            assert(!letters.empty() && letters.size() <= MAX_RACK_TILES);
            Board original = board_from_csv(path.c_str());
            if (std::none_of(std::begin(original.rows), std::end(original.rows),
                             [](const Row &row) { return row.occupiedMask != 0; })) {
                throw std::runtime_error("unreadable or empty post-opening board: " + path);
            }
            check_legal_runs(path.c_str(), words, original);
            Board board = original;
            board.prepare_perpendicular_checks(fixture.lookup);
            SolverBoardSnapshot snapshot{board};
            auto rack = rack_for(letters.c_str());
            for (bool down : {false, true}) {
                check_view(path.c_str(), down ? "down" : "across", words, original, board);
                Decisions decisions = fixture.compare_pass(board, snapshot, down, rack, letters.size());
                std::cout << accepted_count(decisions) << ' ' << decisions.size() << (down ? '\n' : ' ');
                board.swap_views();
                std::swap(original.rows, original.perpendicularRows);
            }
            snapshot.check(board, false);
        }
    }
    words.destroy();
}

int main(int argc, char **argv)
{
    compare_rt_orders(true);
    compare_rt_orders(false);
    compare_rack_groups_and_ruled_out_windows();
    one_tile_counts_in_both_directions_and_restores_the_prepared_board();
    if (argc == 3) {
        qualify_inputs(argv[1], argv[2]);
    } else if (argc != 1) {
        return 1;
    }
}
