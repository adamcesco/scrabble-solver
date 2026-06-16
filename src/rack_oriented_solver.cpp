#include "rack_oriented_solver.h"

#include "utils.h"
#include "validation.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <vector>

#define UINT128_CSTRING_SLOTS 15

static const uint8_t RACK_SUBSET_MASKS_1[] = {
    UINT8_C(0x01), UINT8_C(0x02), UINT8_C(0x04), UINT8_C(0x08), UINT8_C(0x10), UINT8_C(0x20), UINT8_C(0x40)
};
static const uint8_t RACK_SUBSET_MASKS_2[] = {
    UINT8_C(0x03), UINT8_C(0x05), UINT8_C(0x09), UINT8_C(0x11), UINT8_C(0x21), UINT8_C(0x41), UINT8_C(0x06), UINT8_C(0x0A), UINT8_C(0x12), UINT8_C(0x22), UINT8_C(0x42), UINT8_C(0x0C), UINT8_C(0x14), UINT8_C(0x24), UINT8_C(0x44), UINT8_C(0x18), UINT8_C(0x28), UINT8_C(0x48), UINT8_C(0x30), UINT8_C(0x50), UINT8_C(0x60)
};
static const uint8_t RACK_SUBSET_MASKS_3[] = {
    UINT8_C(0x07), UINT8_C(0x0B), UINT8_C(0x13), UINT8_C(0x23), UINT8_C(0x43), UINT8_C(0x0D), UINT8_C(0x15), UINT8_C(0x25), UINT8_C(0x45), UINT8_C(0x19), UINT8_C(0x29), UINT8_C(0x49), UINT8_C(0x31), UINT8_C(0x51), UINT8_C(0x61), UINT8_C(0x0E), UINT8_C(0x16), UINT8_C(0x26), UINT8_C(0x46), UINT8_C(0x1A), UINT8_C(0x2A), UINT8_C(0x4A), UINT8_C(0x32), UINT8_C(0x52), UINT8_C(0x62), UINT8_C(0x1C), UINT8_C(0x2C), UINT8_C(0x4C), UINT8_C(0x34), UINT8_C(0x54), UINT8_C(0x64), UINT8_C(0x38), UINT8_C(0x58), UINT8_C(0x68), UINT8_C(0x70)
};
static const uint8_t RACK_SUBSET_MASKS_4[] = {
    UINT8_C(0x0F), UINT8_C(0x17), UINT8_C(0x27), UINT8_C(0x47), UINT8_C(0x1B), UINT8_C(0x2B), UINT8_C(0x4B), UINT8_C(0x33), UINT8_C(0x53), UINT8_C(0x63), UINT8_C(0x1D), UINT8_C(0x2D), UINT8_C(0x4D), UINT8_C(0x35), UINT8_C(0x55), UINT8_C(0x65), UINT8_C(0x39), UINT8_C(0x59), UINT8_C(0x69), UINT8_C(0x71), UINT8_C(0x1E), UINT8_C(0x2E), UINT8_C(0x4E), UINT8_C(0x36), UINT8_C(0x56), UINT8_C(0x66), UINT8_C(0x3A), UINT8_C(0x5A), UINT8_C(0x6A), UINT8_C(0x72), UINT8_C(0x3C), UINT8_C(0x5C), UINT8_C(0x6C), UINT8_C(0x74), UINT8_C(0x78)
};
static const uint8_t RACK_SUBSET_MASKS_5[] = {
    UINT8_C(0x1F), UINT8_C(0x2F), UINT8_C(0x4F), UINT8_C(0x37), UINT8_C(0x57), UINT8_C(0x67), UINT8_C(0x3B), UINT8_C(0x5B), UINT8_C(0x6B), UINT8_C(0x73), UINT8_C(0x3D), UINT8_C(0x5D), UINT8_C(0x6D), UINT8_C(0x75), UINT8_C(0x79), UINT8_C(0x3E), UINT8_C(0x5E), UINT8_C(0x6E), UINT8_C(0x76), UINT8_C(0x7A), UINT8_C(0x7C)
};
static const uint8_t RACK_SUBSET_MASKS_6[] = {
    UINT8_C(0x3F), UINT8_C(0x5F), UINT8_C(0x6F), UINT8_C(0x77), UINT8_C(0x7B), UINT8_C(0x7D), UINT8_C(0x7E)
};
static const uint8_t RACK_SUBSET_MASKS_7[] = {
    UINT8_C(0x7F)
};

static inline const uint8_t *rack_subset_masks_for_length(uint8_t length, size_t *count)
{
    switch (length) {
        case 1: *count = sizeof(RACK_SUBSET_MASKS_1) / sizeof(RACK_SUBSET_MASKS_1[0]); return RACK_SUBSET_MASKS_1;
        case 2: *count = sizeof(RACK_SUBSET_MASKS_2) / sizeof(RACK_SUBSET_MASKS_2[0]); return RACK_SUBSET_MASKS_2;
        case 3: *count = sizeof(RACK_SUBSET_MASKS_3) / sizeof(RACK_SUBSET_MASKS_3[0]); return RACK_SUBSET_MASKS_3;
        case 4: *count = sizeof(RACK_SUBSET_MASKS_4) / sizeof(RACK_SUBSET_MASKS_4[0]); return RACK_SUBSET_MASKS_4;
        case 5: *count = sizeof(RACK_SUBSET_MASKS_5) / sizeof(RACK_SUBSET_MASKS_5[0]); return RACK_SUBSET_MASKS_5;
        case 6: *count = sizeof(RACK_SUBSET_MASKS_6) / sizeof(RACK_SUBSET_MASKS_6[0]); return RACK_SUBSET_MASKS_6;
        case 7: *count = sizeof(RACK_SUBSET_MASKS_7) / sizeof(RACK_SUBSET_MASKS_7[0]); return RACK_SUBSET_MASKS_7;
        default: *count = 0; return NULL;
    }
}

static inline int canonical_rack_subset_mask(uint64_t sorted_rack, uint8_t mask)
{
    return !(
        (get_byte_64(sorted_rack, 1) == get_byte_64(sorted_rack, 0) && (mask & UINT8_C(0x02)) != 0 && (mask & UINT8_C(0x01)) == 0) ||
        (get_byte_64(sorted_rack, 2) == get_byte_64(sorted_rack, 1) && (mask & UINT8_C(0x04)) != 0 && (mask & UINT8_C(0x02)) == 0) ||
        (get_byte_64(sorted_rack, 3) == get_byte_64(sorted_rack, 2) && (mask & UINT8_C(0x08)) != 0 && (mask & UINT8_C(0x04)) == 0) ||
        (get_byte_64(sorted_rack, 4) == get_byte_64(sorted_rack, 3) && (mask & UINT8_C(0x10)) != 0 && (mask & UINT8_C(0x08)) == 0) ||
        (get_byte_64(sorted_rack, 5) == get_byte_64(sorted_rack, 4) && (mask & UINT8_C(0x20)) != 0 && (mask & UINT8_C(0x10)) == 0) ||
        (get_byte_64(sorted_rack, 6) == get_byte_64(sorted_rack, 5) && (mask & UINT8_C(0x40)) != 0 && (mask & UINT8_C(0x20)) == 0)
    );
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

static inline RowTiles row_window_mask(uint8_t word_length)
{
    RowTiles mask = 0;

    for (uint8_t col_index = 0; col_index < word_length; ++col_index) {
        mask |= row_tile_mask_at_col(col_index);
    }

    return mask;
}

static inline int window_has_vertical_neighbor(const Board *board, uint8_t row_index, uint8_t left, uint8_t right)
{
    const uint16_t span_mask = bit_span_mask_u16(left, right);

    return (row_index > 0 && (board->rows[row_index - 1u].occupiedMask & span_mask) != 0) ||
        (row_index + 1u < BOARD_SIZE && (board->rows[row_index + 1u].occupiedMask & span_mask) != 0);
}

static inline int window_has_left_horizontal_neighbor(const Board *board, uint8_t row_index, uint8_t left)
{
    return left > 0 &&
        (board->rows[row_index].occupiedMask & bit_at_u16((uint8_t)(left - 1u))) != 0;
}

static inline int window_has_right_horizontal_neighbor(const Board *board, uint8_t row_index, uint8_t right)
{
    return right + 1u < BOARD_SIZE &&
        (board->rows[row_index].occupiedMask & bit_at_u16((uint8_t)(right + 1u))) != 0;
}

typedef struct {
    uint8_t row_index;
    uint8_t left;
    uint8_t right;
    uint8_t word_length;
    const WordPattern *pattern_entry;
} RackWindowMatch;

static int validate_rack_window_placement(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const WordIndexStartRowTable *word_start_rows,
    const uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1],
    Board *board,
    const RackWindowMatch *window,
    uint64_t rack_subset,
    Board *copy_of_original_board
)
{
    int count = 0;
    Row row_with_newly_added_word = {};

    const WordPatternAnagramGroup *entry = word_pattern_entry_get_anagram(word_patterns, window->pattern_entry, rack_subset);
    if (entry == NULL) {
        return 0;
    }

    uint32_t start = entry->word_indices.start;
    for (size_t offset = 0; offset < entry->word_indices.count; ++offset) {
        uint32_t word_index = word_patterns->word_indices[(size_t)start + offset];

        const Row *row_with_just_proposed_word = word_index_start_row_table_get(word_start_rows, word_index, window->left);
        if (row_with_just_proposed_word == NULL) {
            continue;
        }

        if (place_word_onto_perpendicular_rows_and_validate(
            dictionary,
            config_to_start_positions,
            copy_of_original_board->perpendicularRows,
            board,
            row_with_just_proposed_word,
            window->row_index,
            window->left,
            window->word_length
        )) {
            ++count;

            add_proposed_word_to_row(&row_with_newly_added_word, &board->rows[window->row_index], row_with_just_proposed_word);
            board->rows[window->row_index] = row_with_newly_added_word;
            // now `board` is the new version of the board that has the new word placed within it

            // board_print(*board);
            // printf("\n");

            // restoring the board back to it's original state
            board->rows[window->row_index] = copy_of_original_board->rows[window->row_index];
        }

        // restoring the board back to it's original state
        memcpy(board->perpendicularRows, copy_of_original_board->perpendicularRows, sizeof(board->perpendicularRows));
    }

    return count;
}

static inline PatternRow pattern_row_for_window(const Board *board, uint8_t row_index, uint8_t left, uint8_t right)
{
    uint8_t word_length = right - left + 1;
    RowTiles window_mask = row_window_mask(word_length);
    uint16_t occupied_mask = low_bit_mask_u16(word_length);

    PatternRow pattern_row = {
        .tiles = (board->rows[row_index].tiles >> row_tile_shift_at_col(left)) & window_mask,
        .keepMask = (board->rows[row_index].careMask >> row_tile_shift_at_col(left)) & window_mask,
        .occupiedMask = (uint16_t)((board->rows[row_index].occupiedMask >> left) & occupied_mask),
        .length = word_length,
    };

    return pattern_row;
}

static void collect_k_zero_slot_window_matches_u128(
    __uint128_t encoded,
    uint8_t rack_pattern_length,
    const WordPatternTable *word_patterns,
    const Board *board,
    uint8_t row_index,
    std::vector<RackWindowMatch> *matches
) {
    const unsigned char *slots = (const unsigned char *)&encoded;

    const uint8_t target = rack_pattern_length;
    const uint8_t target_minus_1 = rack_pattern_length - 1;

    uint8_t leftK = 0;
    uint8_t leftKm1 = 0;

    uint8_t zerosK = 0;
    uint8_t zerosKm1 = 0;

    for (uint8_t right = 0; right < UINT128_CSTRING_SLOTS; right++) {
        if (slots[right] == 0) {
            zerosK++;
            zerosKm1++;
        }

        /*
            Shrink until [leftK, right] has at most target zeros.
        */
        while (zerosK > target) {
            if (slots[leftK] == 0) {
                zerosK--;
            }

            leftK++;
        }

        /*
            Shrink until [leftKm1, right] has at most target - 1 zeros.
        */
        while (zerosKm1 > target_minus_1) {
            if (slots[leftKm1] == 0) {
                zerosKm1--;
            }

            leftKm1++;
        }

        /*
            Starts in [leftK, leftKm1) create windows ending at `right`
            with exactly target zero slots.
        */
        for (uint8_t start = leftK; start < leftKm1; start++) {
            if (window_has_right_horizontal_neighbor(board, row_index, right)) {
                break;
            }
            
            if (window_has_left_horizontal_neighbor(board, row_index, start)) {
                continue;
            }
            
            uint8_t window_length = (uint8_t)(right - start + 1u);
            if (window_length == target &&
                !window_has_vertical_neighbor(board, row_index, start, right)) {
                continue;
            }

            PatternRow pattern_row = pattern_row_for_window(board, row_index, start, right);
            const WordPattern *pattern_entry = word_pattern_table_get(word_patterns, &pattern_row);
            if (pattern_entry != NULL) {
                matches->push_back(RackWindowMatch{
                    row_index,
                    start,
                    right,
                    window_length,
                    pattern_entry
                });
            }
        }
    }
}

size_t rack_oriented_solver(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const WordIndexStartRowTable *word_start_rows,
    uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1],
    Board *board,
    const char *rack_cstring
)
{
    if (dictionary == NULL || word_patterns == NULL || word_start_rows == NULL || board == NULL || rack_cstring == NULL) {
        return 0;
    }

    size_t count = 0;
    Board copy_of_original_board = *board;
    
    uint64_t rack = 0;
    uint8_t rack_length = (uint8_t)strlen(rack_cstring);
    memcpy(&rack, rack_cstring, rack_length);

    for (uint8_t rack_pattern_length = 1; rack_pattern_length <= rack_length; ++rack_pattern_length) {
        size_t pattern_count = 0;
        const uint8_t *pattern_masks = rack_subset_masks_for_length(rack_pattern_length, &pattern_count);
        std::vector<RackWindowMatch> window_matches;

        for (uint8_t row_index = 0; row_index < BOARD_SIZE; ++row_index) {
            collect_k_zero_slot_window_matches_u128(
                board->rows[row_index].tiles,
                rack_pattern_length,
                word_patterns,
                board,
                row_index,
                &window_matches
            );
        }

        for (size_t pattern_index = 0; pattern_index < pattern_count; ++pattern_index) {
            uint8_t pattern_mask = pattern_masks[pattern_index];

            if (!canonical_rack_subset_mask(rack, pattern_mask)) {
                continue;
            }

            uint64_t rack_subset = compact_rack_subset(rack, pattern_mask);
            for (size_t match_index = 0; match_index < window_matches.size(); ++match_index) {
                count += validate_rack_window_placement(
                    dictionary,
                    word_patterns,
                    word_start_rows,
                    config_to_start_positions,
                    board,
                    &window_matches[match_index],
                    rack_subset,
                    &copy_of_original_board
                );
            }
        }
    }

    return count;
}
