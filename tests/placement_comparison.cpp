#include "placement_comparison.h"
#include "placement_trace.h"
#include "square_equivalence.h"

#include <algorithm>
#include <cassert>
#include <ostream>
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

struct VisitedGroup {
    size_t words;
    bool skipped = false;
    unsigned accepted = 0;
    size_t individual_checks = 0;
    size_t gap_checks = 0;
    std::array<size_t, MAX_RACK_TILES> first_failures{};
    bool subset_checked = false;
    unsigned subset_gaps_removed = 0;
    benchmark::SubsetRejection subset_rejection = benchmark::SubsetRejection::none;
};

struct VisitedWindow {
    bool down;
    uint8_t row, start, length, empty;
    bool pattern_match;
    std::vector<VisitedGroup> groups;
    unsigned lookups = 0;
    unsigned cache_hits = 0;
    unsigned masks = 0;
    unsigned retained_gaps = 0;
    bool singleton_letter = false;
    bool ruled_out = false;
    unsigned probes_avoided = 0;
    unsigned merge_comparisons_avoided = 0;
    unsigned rack_gaps_removed = 0;
    unsigned rack_restricted_removed = 0;
};

void WorkloadOutput::write_header() const
{
    stream << "position,variant,down,row,start,length,empty,constrained,pattern_match,"
              "group_size,individual_checks,lookups,cache_hits,masks,retained_gaps,ruled_out,skipped_group,accepted,"
              "probes_avoided,merge_comparisons_avoided,rack_gaps_removed,rack_restricted_removed,"
              "subset_checked,subset_gaps_removed,subset_disjoint,subset_insufficient_capacity,gap_checks";
    for (int position = 1; position <= MAX_RACK_TILES; ++position) {
        stream << ",first_failure_" << position;
    }
    stream << ",singleton_letter\n";
}

struct WindowWorkload {
    std::vector<VisitedWindow> windows;

    void write(WorkloadOutput &output, const char *variant, Board board) const
    {
        bool down = false;
        for (const auto &w : windows) {
            if (w.down != down) {
                board.swap_views();
                down = w.down;
            }
            // Inspect even unmatched windows, after the adapter's lookup trace has ended.
            unsigned constrained = 0;
            for (int col = w.start; col < w.start + w.length; ++col) {
                if (!(board.rows[w.row].occupiedMask & (1u << col)) &&
                    board.perpendicular_check_set(w.row, col) != PerpendicularCheckTable::all_letters) {
                    ++constrained;
                }
            }
            auto emit = [&](const VisitedGroup &group) {
                output.stream << output.position << ',' << variant << ',' << down << ',' << +w.row << ','
                              << +w.start << ',' << +w.length << ',' << +w.empty << ',' << constrained << ','
                              << w.pattern_match << ',' << group.words << ',' << group.individual_checks << ','
                              << w.lookups << ',' << w.cache_hits << ',' << w.masks << ',' << w.retained_gaps << ','
                              << w.ruled_out << ','
                              << group.skipped << ',' << group.accepted << ',' << w.probes_avoided << ','
                              << w.merge_comparisons_avoided << ',' << w.rack_gaps_removed << ','
                              << w.rack_restricted_removed << ',' << group.subset_checked << ','
                              << group.subset_gaps_removed << ','
                              << (group.subset_rejection == benchmark::SubsetRejection::disjoint) << ','
                              << (group.subset_rejection == benchmark::SubsetRejection::capacity) << ','
                              << group.gap_checks;
                for (auto failures : group.first_failures) {
                    output.stream << ',' << failures;
                }
                output.stream << ',' << w.singleton_letter << '\n';
            };
            if (w.groups.empty()) {
                emit({0});
            }
            for (const auto &group : w.groups) {
                emit(group);
            }
        }
    }
};

void trace_window(uint8_t row, uint8_t start, uint8_t length, uint8_t empty, bool pattern_match)
{
    if (active_trace && active_trace->workload) {
        active_trace->workload->windows.push_back({active_trace->down(), row, start, length, empty, pattern_match, {}});
    }
}

static VisitedWindow *current_workload_window()
{
    return active_trace && active_trace->workload ? &active_trace->workload->windows.back() : nullptr;
}

void trace_group(size_t words, bool skipped)
{
    if (auto *window = current_workload_window()) {
        window->groups.push_back({words, skipped});
    }
}

void trace_individual_check()
{
    if (auto *window = current_workload_window()) {
        ++window->groups.back().individual_checks;
    }
}

void trace_gap_check(uint8_t index, bool first_failure)
{
    if (auto *window = current_workload_window()) {
        auto &group = window->groups.back();
        ++group.gap_checks;
        group.first_failures[index] += first_failure;
    }
}

void trace_mask_built(bool ruled_out, unsigned retained_gaps, bool singleton_letter)
{
    if (auto *window = current_workload_window()) {
        ++window->masks;
        window->retained_gaps = retained_gaps;
        window->singleton_letter = singleton_letter;
        window->ruled_out = ruled_out;
    }
}

void trace_skipped_subset_probe()
{
    if (auto *window = current_workload_window()) {
        ++window->probes_avoided;
    }
}

void trace_rack_gap_removed(bool restricted)
{
    if (auto *window = current_workload_window()) {
        ++window->rack_gaps_removed;
        window->rack_restricted_removed += restricted;
    }
}

void trace_subset(unsigned gaps_removed, benchmark::SubsetRejection rejection)
{
    if (auto *window = current_workload_window()) {
        auto &group = window->groups.back();
        group.subset_checked = true;
        group.subset_gaps_removed = gaps_removed;
        group.subset_rejection = rejection;
    }
}

void trace_skipped_merge_comparison()
{
    if (auto *window = current_workload_window()) {
        ++window->merge_comparisons_avoided;
    }
}

void trace_cache_hit()
{
    if (auto *window = current_workload_window()) {
        ++window->cache_hits;
    }
}

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
        if (auto *window = current_workload_window()) {
            ++window->lookups;
        }
    }
}

void trace_placements(uint8_t row, uint8_t start, const uint32_t *words, size_t count, bool accepted)
{
    if (active_trace) {
        if (auto *window = current_workload_window(); window && accepted) {
            window->groups.back().accepted += count;
        }
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
    const std::optional<Decision> &required_play, WorkloadOutput *profile) const
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
    std::array<LookupCounts, 2> neighbor_lookups{};
    std::array<LookupCounts, 2> cached_lookups;
    ViewDecisions sorted_reference;
    for (bool down : {false, true}) {
        check_view(name, down ? "down" : "across", words, original, board);
        active_trace = &reference;
        expected_counts[down] = rack_oriented_full_board_solver(&words, &patterns, &rows, &keys, &board, rack.length);
        active_trace = nullptr;
        snapshot.check(board, down);
        packed_lookups[down] = expected_lookups(board, reference.decisions[down]);
        // Use perpendicular columns to check the filter independently of the solver's row union.
        for (int row = 0; row < BOARD_SIZE; ++row) {
            const unsigned adjacent = ((1u << row) >> 1) | (1u << (row + 1));
            for (int col = 0; col < BOARD_SIZE; ++col) {
                cached_lookups[down][row][col] = packed_lookups[down][row][col] != 0;
                if (board.perpendicularRows[col].occupiedMask & adjacent) {
                    neighbor_lookups[down][row][col] = packed_lookups[down][row][col];
                }
            }
        }
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
        WindowWorkload workload;
        if (profile) {
            actual.workload = &workload;
        }
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
            if (variant == &benchmark::window_variant("packed") || variant == &benchmark::window_variant("empty-bits")) {
                assert(actual.lookups[down] == packed_lookups[down]);
            } else if (variant == &benchmark::window_variant("neighbor-bits")) {
                assert(actual.lookups[down] == neighbor_lookups[down]);
            } else if (variant == &benchmark::window_variant("row-cache")) {
                assert(actual.lookups[down] == cached_lookups[down]);
            }
        }
        if (profile) {
            workload.write(*profile, variant->name, board);
        }
    }
    // Fixtures can inspect the order in which the oracle encountered the candidates.
    return reference.decisions;
}
