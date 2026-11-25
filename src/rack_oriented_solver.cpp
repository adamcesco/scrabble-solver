#include "rack_oriented_solver.h"

#include "row_windows.h"
#include "utils.h"
#include "validation_hot.h"

#include <algorithm>
#include <optional>
#include <stdint.h>

#ifdef SCRABBLE_WINDOW_VARIANTS
#include "window_variants.h"
#include <chrono>
#include <string>
#endif

#ifdef SCRABBLE_TRACE_PLACEMENTS
#include "placement_trace.h"
#else
static inline void trace_placements(uint8_t, uint8_t, const uint32_t *, size_t, bool) {}
static inline void trace_window(uint8_t, uint8_t, uint8_t, uint8_t, bool) {}
static inline void trace_group(size_t) {}
static inline void trace_individual_check() {}
#endif

static inline int canonical_rack_subset_mask(uint8_t duplicate_follow_mask, uint8_t mask)
{
    return (duplicate_follow_mask & mask & (uint8_t)~(mask << 1u)) == 0;
}

static inline uint64_t compact_rack_subset(uint64_t sorted_rack, uint8_t mask)
{
    uint8_t k0 = (uint8_t)((mask >> 0u) & 1u);
    uint8_t k1 = (uint8_t)((mask >> 1u) & 1u);
    uint8_t k2 = (uint8_t)((mask >> 2u) & 1u);
    uint8_t k3 = (uint8_t)((mask >> 3u) & 1u);
    uint8_t k4 = (uint8_t)((mask >> 4u) & 1u);
    uint8_t k5 = (uint8_t)((mask >> 5u) & 1u);
    uint8_t k6 = (uint8_t)((mask >> 6u) & 1u);

    uint8_t m0 = (uint8_t)(0u - k0);
    uint8_t m1 = (uint8_t)(0u - k1);
    uint8_t m2 = (uint8_t)(0u - k2);
    uint8_t m3 = (uint8_t)(0u - k3);
    uint8_t m4 = (uint8_t)(0u - k4);
    uint8_t m5 = (uint8_t)(0u - k5);
    uint8_t m6 = (uint8_t)(0u - k6);

    unsigned p0 = 0;
    unsigned p1 = k0;
    unsigned p2 = k0 + k1;
    unsigned p3 = k0 + k1 + k2;
    unsigned p4 = k0 + k1 + k2 + k3;
    unsigned p5 = k0 + k1 + k2 + k3 + k4;
    unsigned p6 = k0 + k1 + k2 + k3 + k4 + k5;

    return put_byte_64((uint8_t)(get_byte_64(sorted_rack, 0) & m0), p0) |
        put_byte_64((uint8_t)(get_byte_64(sorted_rack, 1) & m1), p1) |
        put_byte_64((uint8_t)(get_byte_64(sorted_rack, 2) & m2), p2) |
        put_byte_64((uint8_t)(get_byte_64(sorted_rack, 3) & m3), p3) |
        put_byte_64((uint8_t)(get_byte_64(sorted_rack, 4) & m4), p4) |
        put_byte_64((uint8_t)(get_byte_64(sorted_rack, 5) & m5), p5) |
        put_byte_64((uint8_t)(get_byte_64(sorted_rack, 6) & m6), p6);
}

static inline uint8_t duplicate_follow_mask_for_rack(uint64_t sorted_rack, uint8_t rack_length)
{
    uint8_t duplicate_follow_mask = 0;

    for (uint8_t index = 1; index < rack_length; ++index) {
        if (get_byte_64(sorted_rack, index) == get_byte_64(sorted_rack, (uint8_t)(index - 1u))) {
            duplicate_follow_mask |= (uint8_t)(UINT8_C(1) << index);
        }
    }

    return duplicate_follow_mask;
}

RackSubsetKeys rack_subset_keys_for_rack(uint64_t sorted_rack, uint8_t rack_length)
{
    RackSubsetKeys subset_keys = {};
    const uint8_t duplicate_follow_mask = duplicate_follow_mask_for_rack(sorted_rack, rack_length);
    const uint8_t subset_count = (uint8_t)(UINT8_C(1) << rack_length);

    for (uint8_t mask = 1; mask < subset_count; ++mask) {
        if (!canonical_rack_subset_mask(duplicate_follow_mask, mask)) {
            continue;
        }

        uint8_t key_length = (uint8_t)__builtin_popcount(mask);
        uint8_t key_index = subset_keys.counts[key_length]++;

        subset_keys.keys[key_length][key_index] = compact_rack_subset(sorted_rack, mask);
    }

    for (uint8_t key_length = 1; key_length <= rack_length; ++key_length) {
        std::sort(
            subset_keys.keys[key_length],
            subset_keys.keys[key_length] + subset_keys.counts[key_length]
        );
    }

    return subset_keys;
}

static inline void restore_perpendicular_window(
    Board *board,
    const Board *copy_of_original_board,
    uint16_t touched_columns
)
{
    while (touched_columns != 0) {
        uint8_t col_index = (uint8_t)__builtin_ctz(touched_columns);
        board->perpendicularRows[col_index] = copy_of_original_board->perpendicularRows[col_index];
        touched_columns &= (uint16_t)(touched_columns - 1u);
    }
}

static inline int validate_rack_window_words(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const WordIndexStartRowTable *word_start_rows,
    Board *board,
    uint8_t row_index,
    uint8_t start_col,
    uint8_t word_length,
    const WordPatternAnagramGroup &entry,
    const Board *copy_of_original_board
)
{
    int count = 0;

    uint32_t start = entry.word_indices.start;
    for (size_t offset = 0; offset < entry.word_indices.count; ++offset) {
        const uint32_t &word_index = word_patterns->word_indices[(size_t)start + offset];

        const Row *row_with_just_proposed_word = word_start_rows->get_unchecked(word_index, start_col);

        uint16_t touched_columns = 0;
        bool accepted = hot_place_word_onto_perpendicular_rows_and_validate_with_touched_mask(
            dictionary,
            copy_of_original_board->perpendicularRows,
            board,
            row_with_just_proposed_word,
            row_index,
            start_col,
            word_length,
            &touched_columns
        );
        count += accepted;

        restore_perpendicular_window(board, copy_of_original_board, touched_columns);
        trace_placements(row_index, start_col, &word_index, 1, accepted);
    }

    return count;
}

template <typename Visitor>
static void visit_matching_anagrams(
    const WordPatternTable &word_patterns,
    const WordPattern &match,
    const uint64_t *rack_subsets,
    uint8_t rack_subset_count,
    Visitor visit
)
{
    const WordPatternRange anagrams = match.anagrams;
    if (anagrams.count > (uint32_t)rack_subset_count * 4u) {
        for (uint8_t index = 0; index < rack_subset_count; ++index) {
            const WordPatternAnagramGroup *group = word_patterns.get_anagram(match, rack_subsets[index]);
            if (group != nullptr) {
                trace_group(group->word_indices.count);
                visit(*group);
            }
        }
        return;
    }

    const WordPatternAnagramGroup *groups = word_patterns.anagram_groups.data() + anagrams.start;
    uint32_t group_index = 0;
    uint8_t rack_subset_index = 0;

    while (group_index < anagrams.count && rack_subset_index < rack_subset_count) {
        const PatternBytes group_key = groups[group_index].rack_letters;
        const PatternBytes rack_key = rack_subsets[rack_subset_index];

        if (group_key < rack_key) {
            ++group_index;
        } else if (rack_key < group_key) {
            ++rack_subset_index;
        } else {
            trace_group(groups[group_index].word_indices.count);
            visit(groups[group_index]);
            ++group_index;
            ++rack_subset_index;
        }
    }
}

namespace {
struct MatchedWindow {
    uint8_t start;
    uint8_t length;
    const WordPattern *match;
};

enum class GapTraversal { scan, empty_bits, neighbor_bits };

template <GapTraversal traversal>
struct WindowMasks {
    struct Gap {
        uint8_t offset;
        uint32_t letters;
    };
    Gap gaps[MAX_RACK_TILES];
    uint8_t gap_count = 0;
    bool ruled_out = false;

    WindowMasks(const Board &board, uint8_t row, const MatchedWindow &window, uint16_t eligible_columns = UINT16_MAX)
    {
        auto add_gap = [&](uint8_t offset, uint32_t letters) {
            gaps[gap_count++] = {offset, letters};
            ruled_out |= letters == 0;
        };
        if constexpr (traversal == GapTraversal::scan) {
            for (uint8_t offset = 0; offset < window.length; ++offset) {
                uint8_t col = window.start + offset;
                if (!(board.rows[row].occupiedMask & bit_at_u16(col))) {
                    add_gap(offset, board.perpendicular_check_set(row, col));
                }
            }
        } else {
            const uint16_t span = low_bit_mask_u16(window.length) << window.start;
            uint16_t remaining = span & ~board.rows[row].occupiedMask & eligible_columns;
            while (remaining) {
                const uint8_t col = __builtin_ctz(remaining);
                add_gap(col - window.start, board.perpendicular_check_set(row, col));
                remaining &= remaining - 1u;
            }
        }
    }

    bool accepts(const char *word) const
    {
        trace_individual_check();
        for (uint8_t i = 0; i < gap_count; ++i) {
            const Gap &gap = gaps[i];
            if (gap.letters != PerpendicularCheckTable::all_letters &&
                !(gap.letters & (1u << (word[gap.offset] - 'A')))) {
                return false;
            }
        }
        return true;
    }
};

template <typename CountWindow>
size_t count_rack_windows(
    const WordPatternTable *word_patterns,
    const RackSubsetKeys *rack_subset_keys,
    const Board *board,
    uint8_t rack_length,
    CountWindow count_window
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
        for (uint8_t empty_count = 1; empty_count <= rack_length; ++empty_count) {
            for (uint8_t index = 0; index < counts[empty_count]; ++index) {
                const MatchedWindow &window = by_empty_count[empty_count][index];
                trace_window(row_index, window.start, window.length, empty_count, true);
                auto visit_groups = [&](auto visit) {
                    visit_matching_anagrams(
                        *word_patterns, *window.match,
                        rack_subset_keys->keys[empty_count], rack_subset_keys->counts[empty_count], visit
                    );
                };
                count += count_window(row_index, neighbors, window, empty_count, visit_groups);
            }
        }
    }
    return count;
}

template <GapTraversal traversal>
struct WindowCounter {
    const WordTable &dictionary;
    const WordPatternTable &patterns;
    const Board &board;

    WindowCounter(const WordTable &dictionary, const WordPatternTable &patterns, const Board &board)
        : dictionary(dictionary), patterns(patterns), board(board) {}

    template <typename VisitGroups>
    size_t operator()(uint8_t row, uint16_t neighbors, const MatchedWindow &window, uint8_t, VisitGroups visit_groups) const
    {
        size_t count = 0;
        std::optional<WindowMasks<traversal>> masks;
        visit_groups([&](const WordPatternAnagramGroup &group) {
            if (!masks) {
                if constexpr (traversal == GapTraversal::neighbor_bits) {
                    masks.emplace(board, row, window, neighbors);
                } else {
                    masks.emplace(board, row, window);
                }
            }
            const uint32_t *ids = patterns.word_indices.data() + group.word_indices.start;
            if (masks->ruled_out) {
                trace_placements(row, window.start, ids, group.word_indices.count, false);
                return;
            }
            for (size_t i = 0; i < group.word_indices.count; ++i) {
                bool accepted = masks->accepts(dictionary.words[ids[i]]);
                count += accepted;
                trace_placements(row, window.start, ids + i, 1, accepted);
            }
        });
        return count;
    }
};

using PackedCounter = WindowCounter<GapTraversal::scan>;

#ifdef SCRABBLE_WINDOW_VARIANTS
struct OneGapCounter : PackedCounter {
    using PackedCounter::PackedCounter;

    template <typename VisitGroups>
    size_t operator()(uint8_t row, uint16_t neighbors, const MatchedWindow &window, uint8_t empty_count,
                      VisitGroups visit_groups) const
    {
        if (empty_count != 1) {
            return PackedCounter::operator()(row, neighbors, window, empty_count, visit_groups);
        }
        std::optional<uint32_t> letters;
        size_t count = 0;
        visit_groups([&](const WordPatternAnagramGroup &group) {
            if (!letters) {
                const auto span = low_bit_mask_u16(window.length) << window.start;
                const auto gap = span & ~board.rows[row].occupiedMask;
                letters = board.perpendicular_check_set(row, __builtin_ctz(gap));
            }
            const auto tile = get_byte_128(group.rack_letters, 0, BOARD_SIZE);
            const bool accepted = *letters & (1u << (tile - 'A'));
            if (accepted) {
                count += group.word_indices.count;
            }
            trace_placements(row, window.start, patterns.word_indices.data() + group.word_indices.start,
                             group.word_indices.count, accepted);
        });
        return count;
    }
};

template <typename Counter>
benchmark::WindowSolve measure_solve(const WordTable &words, const WordPatternTable &patterns,
                                     Board &board, const benchmark::Rack &rack)
{
#ifndef SCRABBLE_TRACE_PLACEMENTS
    using Clock = std::chrono::steady_clock;
    auto start = Clock::now();
#endif
    auto keys = rack_subset_keys_for_rack(rack.tiles, rack.length);
    Counter counter(words, patterns, board);
    auto counts = count_both_views(board, [&] {
        return count_rack_windows(&patterns, &keys, &board, rack.length, counter);
    });
#ifdef SCRABBLE_TRACE_PLACEMENTS
    return {counts, 0};
#else
    auto end = Clock::now();
    return {counts, std::chrono::duration<double, std::micro>(end - start).count()};
#endif
}
#endif
}

#ifdef SCRABBLE_WINDOW_VARIANTS
const benchmark::WindowVariant &benchmark::window_variant(std::string_view name)
{
    static constexpr WindowVariant variants[] = {
        {"packed", measure_solve<PackedCounter>},
        {"one-gap", measure_solve<OneGapCounter>},
        {"empty-bits", measure_solve<WindowCounter<GapTraversal::empty_bits>>},
        {"neighbor-bits", measure_solve<WindowCounter<GapTraversal::neighbor_bits>>},
    };
    for (const auto &variant : variants) {
        if (variant.name == name) {
            return variant;
        }
    }
    throw std::invalid_argument("unknown window counter: " + std::string(name));
}
#endif

size_t rack_oriented_full_board_solver(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const RackSubsetKeys *rack_subset_keys,
    const Board *board,
    uint8_t rack_length
)
{
    return count_rack_windows(word_patterns, rack_subset_keys, board, rack_length,
                              PackedCounter(*dictionary, *word_patterns, *board));
}

size_t rack_oriented_full_board_solver(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const WordIndexStartRowTable *word_start_rows,
    RackSubsetKeys *rack_subset_keys,
    Board *board,
    uint8_t rack_length
)
{
    const Board original_board = *board;
    return count_rack_windows(word_patterns, rack_subset_keys, board, rack_length,
        [&](uint8_t row, uint16_t, const MatchedWindow &window, uint8_t, auto visit_groups) {
            size_t count = 0;
            visit_groups([&](const WordPatternAnagramGroup &group) {
                count += validate_rack_window_words(
                    dictionary, word_patterns, word_start_rows, board,
                    row, window.start, window.length, group, &original_board
                );
            });
            return count;
        });
}

DirectionalCounts rack_oriented_both_directions(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const RackSubsetKeys *rack_subset_keys,
    Board *board,
    uint8_t rack_length
)
{
    return count_both_views(*board, [&] {
        return rack_oriented_full_board_solver(dictionary, word_patterns, rack_subset_keys, board, rack_length);
    });
}
