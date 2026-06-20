#include "rack_oriented_solver.h"

#include "utils.h"
#include "validation_hot.h"

#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_RACK_WINDOW_MATCHES (BOARD_SIZE * ((BOARD_SIZE * (BOARD_SIZE + 1)) / 2))

#if defined(__GNUC__) || defined(__clang__)
#define SOLVER_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define SOLVER_ALWAYS_INLINE inline
#endif

static constexpr RowTiles row_window_mask_for_length(unsigned word_length)
{
    return (((RowTiles)1u << (word_length * ROW_TILE_BITS)) - 1u);
}

static constexpr uint16_t low_bit_mask_for_length(unsigned bit_count)
{
    return (uint16_t)((UINT16_C(1) << bit_count) - 1u);
}

static constexpr RowTiles ROW_WINDOW_MASKS[BOARD_SIZE + 1] = {
    row_window_mask_for_length(0),
    row_window_mask_for_length(1),
    row_window_mask_for_length(2),
    row_window_mask_for_length(3),
    row_window_mask_for_length(4),
    row_window_mask_for_length(5),
    row_window_mask_for_length(6),
    row_window_mask_for_length(7),
    row_window_mask_for_length(8),
    row_window_mask_for_length(9),
    row_window_mask_for_length(10),
    row_window_mask_for_length(11),
    row_window_mask_for_length(12),
    row_window_mask_for_length(13),
    row_window_mask_for_length(14),
    row_window_mask_for_length(15),
};

static constexpr uint16_t LOW_BIT_MASKS[BOARD_SIZE + 1] = {
    low_bit_mask_for_length(0),
    low_bit_mask_for_length(1),
    low_bit_mask_for_length(2),
    low_bit_mask_for_length(3),
    low_bit_mask_for_length(4),
    low_bit_mask_for_length(5),
    low_bit_mask_for_length(6),
    low_bit_mask_for_length(7),
    low_bit_mask_for_length(8),
    low_bit_mask_for_length(9),
    low_bit_mask_for_length(10),
    low_bit_mask_for_length(11),
    low_bit_mask_for_length(12),
    low_bit_mask_for_length(13),
    low_bit_mask_for_length(14),
    low_bit_mask_for_length(15),
};

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

typedef struct {
    uint8_t row_index;
    uint8_t left;
    uint8_t right;
    uint8_t word_length;
    const WordPattern *pattern_entry;
} RackWindowMatch;

typedef struct {
    RackWindowMatch items[MAX_RACK_TILES + 1][MAX_RACK_WINDOW_MATCHES];
    size_t counts[MAX_RACK_TILES + 1];
} RackWindowMatches; // todo: eventually organize this by row, so that when you solve subsequent boards, you only need recalculate the window matches for that row

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
    const uint32_t *word_indices,
    const WordIndexStartRowTable *word_start_rows,
    Board *board,
    const RackWindowMatch *window,
    const WordPatternAnagramGroup *entry,
    Board *copy_of_original_board
)
{
    int count = 0;
    Row row_with_newly_added_word = {};

    uint32_t start = entry->word_indices.start;
    for (size_t offset = 0; offset < entry->word_indices.count; ++offset) {
        uint32_t word_index = word_indices[(size_t)start + offset];

        const Row *row_with_just_proposed_word = word_start_rows->get_unchecked(word_index, window->left);

        uint16_t touched_columns = 0;
        if (hot_place_word_onto_perpendicular_rows_and_validate_with_touched_mask(
            dictionary,
            copy_of_original_board->perpendicularRows,
            board,
            row_with_just_proposed_word,
            window->row_index,
            window->left,
            window->word_length,
            &touched_columns
        )) {
            ++count;

            add_proposed_word_to_row(&row_with_newly_added_word, &board->rows[window->row_index], row_with_just_proposed_word);
            board->rows[window->row_index] = row_with_newly_added_word;
            board->rows[window->row_index] = copy_of_original_board->rows[window->row_index];
        }

        restore_perpendicular_window(board, copy_of_original_board, touched_columns);
    }

    return count;
}

static int validate_rack_window_placement(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const WordIndexStartRowTable *word_start_rows,
    Board *board,
    const RackWindowMatch *window,
    uint64_t rack_subset,
    Board *copy_of_original_board
)
{
    const WordPatternAnagramGroup *entry = word_patterns->get_anagram(*window->pattern_entry, rack_subset);
    if (entry == NULL) {
        return 0;
    }

    return validate_rack_window_words(
        dictionary,
        word_patterns->word_indices.data(),
        word_start_rows,
        board,
        window,
        entry,
        copy_of_original_board
    );
}

static int validate_rack_window_placement_merge(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const WordIndexStartRowTable *word_start_rows,
    Board *board,
    const RackWindowMatch *window,
    const uint64_t *rack_subsets,
    uint8_t rack_subset_count,
    Board *copy_of_original_board
)
{
    int count = 0;
    const WordPatternRange anagrams = window->pattern_entry->anagrams;
    const WordPatternAnagramGroup *groups = word_patterns->anagram_groups.data() + anagrams.start;
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
            count += validate_rack_window_words(
                dictionary,
                word_patterns->word_indices.data(),
                word_start_rows,
                board,
                window,
                &groups[group_index],
                copy_of_original_board
            );
            ++group_index;
            ++rack_subset_index;
        }
    }

    return count;
}

static SOLVER_ALWAYS_INLINE void collect_window_matches_for_all_rack_lengths_u128(
    __uint128_t encoded,
    uint16_t row_occupied,
    uint16_t above_occupied,
    uint16_t below_occupied,
    uint8_t rack_length,
    const WordPatternTable *word_patterns,
    uint8_t row_index,
    RackWindowMatches *matches
) {
    const uint16_t neighbor_occupied = (uint16_t)(above_occupied | below_occupied);

    for (uint8_t start = 0; start < BOARD_SIZE; ++start) {
        if (start > 0 && (row_occupied & bit_at_u16((uint8_t)(start - 1u))) != 0) {
            continue;
        }

        uint8_t zero_count = 0;

        for (uint8_t right = start; right < BOARD_SIZE; ++right) {
            if ((row_occupied & bit_at_u16(right)) == 0) {
                ++zero_count;
                if (zero_count > rack_length) {
                    break;
                }
            }

            if (right + 1u < BOARD_SIZE && (row_occupied & bit_at_u16((uint8_t)(right + 1u))) != 0) {
                continue;
            }

            if (zero_count == 0) {
                continue;
            }

            const uint8_t word_length = (uint8_t)(right - start + 1u);
            const uint16_t window_mask = bit_span_mask_u16(start, right);

            if (word_length == zero_count && (neighbor_occupied & window_mask) == 0) {
                continue;
            }

            PatternRow pattern_row = {
                .tiles = (encoded >> row_tile_shift_at_col(start)) & ROW_WINDOW_MASKS[word_length],
                .occupiedMask = (uint16_t)((row_occupied >> start) & LOW_BIT_MASKS[word_length]),
                .length = word_length,
            };
            const WordPattern *pattern_entry = word_patterns->get(pattern_row);
            if (pattern_entry != NULL) {
                size_t match_index = matches->counts[zero_count]++;
                matches->items[zero_count][match_index] = RackWindowMatch{
                    row_index,
                    start,
                    right,
                    word_length,
                    pattern_entry
                };
            }
        }
    }
}

size_t rack_oriented_solver(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const WordIndexStartRowTable *word_start_rows,
    RackSubsetKeys *rack_subset_keys,
    Board *board,
    uint8_t rack_length
)
{
    size_t count = 0;
    Board copy_of_original_board = *board;
    RackWindowMatches window_matches;

    memset(window_matches.counts, 0, sizeof(window_matches.counts));
    for (uint8_t row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        const Row *row = &board->rows[row_index];
        collect_window_matches_for_all_rack_lengths_u128(
            row->tiles,
            row->occupiedMask,
            row_index > 0 ? board->rows[row_index - 1u].occupiedMask : 0,
            row_index + 1u < BOARD_SIZE ? board->rows[row_index + 1u].occupiedMask : 0,
            rack_length,
            word_patterns,
            row_index,
            &window_matches
        );
    }

    for (uint8_t rack_pattern_length = 1; rack_pattern_length <= rack_length; ++rack_pattern_length) {
        uint8_t rack_subset_count = rack_subset_keys->counts[rack_pattern_length];

        for (size_t match_index = 0; match_index < window_matches.counts[rack_pattern_length]; ++match_index) {
            const RackWindowMatch *window = &window_matches.items[rack_pattern_length][match_index];
            const uint64_t *rack_subsets = rack_subset_keys->keys[rack_pattern_length];

            if (window->pattern_entry->anagrams.count <= (uint32_t)rack_subset_count * 4u) {
                count += validate_rack_window_placement_merge(
                    dictionary,
                    word_patterns,
                    word_start_rows,
                    board,
                    window,
                    rack_subsets,
                    rack_subset_count,
                    &copy_of_original_board
                );
            } else {
                for (uint8_t rack_subset_index = 0; rack_subset_index < rack_subset_count; ++rack_subset_index) {
                    uint64_t rack_subset = rack_subsets[rack_subset_index];

                    count += validate_rack_window_placement(
                        dictionary,
                        word_patterns,
                        word_start_rows,
                        board,
                        window,
                        rack_subset,
                        &copy_of_original_board
                    );
                }
            }
        }
    }

    return count;
}
