#ifndef SRC_SOLVERS_SHARED_RACK_WINDOWS_H
#define SRC_SOLVERS_SHARED_RACK_WINDOWS_H

#include "placement_tracing.h"
#include "row_windows.h"
#include "solvers/shared.h"
#include "word_pattern_table.h"

namespace rack_solver {
struct MatchedWindow {
    uint8_t start;
    uint8_t length;
    const WordPattern *match;

    uint16_t empty_columns(uint16_t occupied) const
    {
        return (low_bit_mask_u16(length) << start) & ~occupied;
    }
};

enum class GroupSearch { keep_going, stop_window };

template <typename Visitor>
void visit_matching_anagrams(
    const WordPatternTable &word_patterns,
    uint8_t row, const MatchedWindow &window,
    const uint64_t *rack_subsets,
    uint8_t rack_subset_count,
    Visitor visit
)
{
    bool stopped = false;
    auto visit_group_and_check_stop = [&](const WordPatternAnagramGroup &group) {
        trace_group(group.word_indices.count, stopped);
        if (stopped) {
            const uint32_t *ids = word_patterns.word_indices.data() + group.word_indices.start;
            trace_placements(row, window.start, ids, group.word_indices.count, false);
        } else if (visit(group) == GroupSearch::stop_window) {
#ifdef SCRABBLE_TRACE_PLACEMENTS
            // Enumerate the remaining matches solely to qualify their rejections.
            stopped = true;
#else
            return true;
#endif
        }
        return false;
    };
    const WordPatternRange anagrams = window.match->anagrams;
    if (anagrams.count > (uint32_t)rack_subset_count * 4u) {
        for (uint8_t index = 0; index < rack_subset_count; ++index) {
            if (stopped) {
                trace_skipped_subset_probe();
            }
            const WordPatternAnagramGroup *group = word_patterns.get_anagram(*window.match, rack_subsets[index]);
            if (group != nullptr && visit_group_and_check_stop(*group)) {
                return;
            }
        }
        return;
    }

    const WordPatternAnagramGroup *groups = word_patterns.anagram_groups.data() + anagrams.start;
    uint32_t group_index = 0;
    uint8_t rack_subset_index = 0;

    while (group_index < anagrams.count && rack_subset_index < rack_subset_count) {
        if (stopped) {
            trace_skipped_merge_comparison();
        }
        const PatternBytes group_key = groups[group_index].rack_letters;
        const PatternBytes rack_key = rack_subsets[rack_subset_index];

        if (group_key < rack_key) {
            ++group_index;
        } else if (rack_key < group_key) {
            ++rack_subset_index;
        } else {
            if (visit_group_and_check_stop(groups[group_index])) {
                return;
            }
            ++group_index;
            ++rack_subset_index;
        }
    }
}

struct RowCheckSets {
    const Board &board;
    const uint8_t row;

    RowCheckSets(const Board &board, uint8_t row) : board(board), row(row) {}
    uint16_t occupied() const { return board.rows[row].occupiedMask; }
    uint32_t get(uint8_t col) const { return board.perpendicular_check_set(row, col); }
};

template <typename Checks = RowCheckSets, typename CountWindow>
size_t count_rack_windows(
    const WordPatternTable *word_patterns,
    const RackSubsetKeys *rack_subset_keys,
    const Board *board,
    uint8_t rack_length,
    const CountWindow &count_window
)
{
    size_t count = 0;
    for (uint8_t row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        // Each empty count has at most one complete window per start column:
        // its end must be just before the next empty square or at the board edge.
        MatchedWindow by_empty_count[MAX_RACK_TILES + 1][BOARD_SIZE];
        uint8_t counts[MAX_RACK_TILES + 1] = {};
        const uint16_t neighbors = visit_row_windows(
            board->rows, row_index, rack_length,
            [&](uint8_t start, const PatternRow &pattern, uint8_t empty_count) {
                const WordPattern *match = word_patterns->get(pattern);
                if (match != nullptr) {
                    by_empty_count[empty_count][counts[empty_count]++] = {start, pattern.length, match};
                } else {
                    trace_window(row_index, start, pattern.length, empty_count, false);
                }
            }
        );
        Checks checks(*board, row_index);
        for (uint8_t empty_count = 1; empty_count <= rack_length; ++empty_count) {
            for (uint8_t index = 0; index < counts[empty_count]; ++index) {
                const MatchedWindow &window = by_empty_count[empty_count][index];
                trace_window(row_index, window.start, window.length, empty_count, true);
                auto visit_groups = [&](auto visit) {
                    visit_matching_anagrams(
                        *word_patterns, row_index, window,
                        rack_subset_keys->keys[empty_count], rack_subset_keys->counts[empty_count], visit
                    );
                };
                count += count_window(checks, neighbors, window, empty_count, visit_groups);
            }
        }
    }
    return count;
}

} // namespace rack_solver

#endif
