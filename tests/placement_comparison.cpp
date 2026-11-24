#include "placement_comparison.h"
#include "placement_trace.h"
#include "square_equivalence.h"

#include <algorithm>
#include <cassert>
#include <set>
#include <stdexcept>

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

bool SolverBoardSnapshot::is_down(const Board &board) const
{
    return board.normal_runs == original.perpendicular_runs;
}

static PlacementTrace *active_trace;

DirectionalCounts PlacementTrace::solve(const benchmark::WindowVariant &variant, const WordPatternTable &patterns,
                                        const benchmark::Rack &rack)
{
    active_trace = this;
    auto counts = variant.solve(dictionary, patterns, board, rack).counts;
    active_trace = nullptr;
    snapshot.check(board, false);
    return counts;
}

void trace_square_lookup(uint8_t row, uint8_t col)
{
    if (active_trace) {
        ++active_trace->lookups[active_trace->down()][row][col];
    }
}

void trace_placements(uint8_t row, uint8_t start, const uint32_t *words, size_t count, bool accepted)
{
    if (active_trace) {
        const bool down = active_trace->down();
        active_trace->snapshot.check(active_trace->board, down);
        for (size_t i = 0; i < count; ++i) {
            active_trace->decisions[down].emplace_back(down, row, start,
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

ViewDecisions PlacementComparison::compare_board(
    const char *name, Board original, std::string letters, const WindowVariants &variants,
    const std::optional<Decision> &required_play) const
{
    check_legal_runs(name, words, original);
    Board board = original;
    board.prepare_perpendicular_checks(lookup);
    const SolverBoardSnapshot snapshot{board};
    std::sort(letters.begin(), letters.end());
    const benchmark::Rack rack(letters);
    auto keys = rack_subset_keys_for_rack(rack.tiles, rack.length);
    PlacementTrace reference{words, board, snapshot};
    std::array<size_t, 2> expected_counts;
    std::array<LookupCounts, 2> packed_lookups;
    ViewDecisions sorted_reference;
    for (bool down : {false, true}) {
        check_view(name, down ? "down" : "across", words, original, board);
        active_trace = &reference;
        expected_counts[down] = rack_oriented_full_board_solver(&words, &patterns, &rows, &keys, &board, rack.length);
        active_trace = nullptr;
        snapshot.check(board, down);
        packed_lookups[down] = expected_lookups(board, reference.decisions[down]);
        sorted_reference[down] = reference.decisions[down];
        std::sort(sorted_reference[down].begin(), sorted_reference[down].end());
        board.swap_views();
        original.swap_views();
    }
    snapshot.check(board, false);
    if (required_play) {
        const auto &[down, row, start, word, accepted] = *required_play;
        const auto &decisions = sorted_reference[down];
        if (!std::binary_search(decisions.begin(), decisions.end(), *required_play)) {
            throw std::runtime_error(std::string(name) + ": solver did not accept recorded play " + word);
        }
    }

    for (const auto *variant : variants) {
        PlacementTrace actual{words, board, snapshot};
        auto counts = actual.solve(*variant, patterns, rack);
        for (bool down : {false, true}) {
            auto &sorted = actual.decisions[down];
            std::sort(sorted.begin(), sorted.end());
            const size_t count = down ? counts.down : counts.across;
            if (sorted != sorted_reference[down] || count != accepted_count(sorted) ||
                count != expected_counts[down]) {
                throw std::runtime_error(std::string(name) + " " + variant->name +
                                         (down ? " down" : " across") + ": placement mismatch");
            }
            if (variant == &benchmark::window_variant("packed")) {
                assert(actual.lookups[down] == packed_lookups[down]);
            }
        }
    }
    // Fixtures can inspect the order in which the oracle encountered the candidates.
    return reference.decisions;
}
