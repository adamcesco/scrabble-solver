#include "placement_comparison.h"
#include "placement_trace.h"
#include "square_equivalence.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <set>

using LookupCounts = std::array<std::array<unsigned, BOARD_SIZE>, BOARD_SIZE>;

static void check_rows(const Row (&expected)[BOARD_SIZE], const Row (&actual)[BOARD_SIZE])
{
    for (int i = 0; i < BOARD_SIZE; ++i) {
        assert(actual[i].tiles == expected[i].tiles);
        assert(actual[i].occupiedMask == expected[i].occupiedMask);
        assert(actual[i].careMask == expected[i].careMask);
    }
}

void SolverBoardSnapshot::check(const Board &board, bool down) const
{
    check_rows(down ? original.perpendicularRows : original.rows, board.rows);
    check_rows(down ? original.rows : original.perpendicularRows, board.perpendicularRows);
    // These pointers identify immutable grids, including IDs that no candidate reads.
    assert(board.normal_runs == (down ? original.perpendicular_runs : original.normal_runs));
    assert(board.perpendicular_runs == (down ? original.normal_runs : original.perpendicular_runs));
}

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

size_t accepted_count(const Decisions &decisions)
{
    return std::count_if(decisions.begin(), decisions.end(), [](const Decision &decision) {
        const auto &[down, row, start, word, accepted] = decision;
        return accepted;
    });
}

Decisions PlacementComparison::compare_pass(Board &board, const SolverBoardSnapshot &snapshot, bool down,
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

RackSubsetKeys rack_for(const char *letters)
{
    uint64_t packed = 0;
    memcpy(&packed, letters, strlen(letters));
    return rack_subset_keys_for_rack(packed, strlen(letters));
}

std::array<PassCounts, 2> PlacementComparison::compare_board(
    const char *name, Board original, const std::string &letters) const
{
    check_legal_runs(name, words, original);
    Board board = original;
    board.prepare_perpendicular_checks(lookup);
    SolverBoardSnapshot snapshot{board};
    auto rack = rack_for(letters.c_str());
    std::array<PassCounts, 2> counts;
    for (bool down : {false, true}) {
        check_view(name, down ? "down" : "across", words, original, board);
        auto decisions = compare_pass(board, snapshot, down, rack, letters.size());
        counts[down] = {accepted_count(decisions), decisions.size()};
        board.swap_views();
        std::swap(original.rows, original.perpendicularRows);
    }
    snapshot.check(board, false);
    return counts;
}
