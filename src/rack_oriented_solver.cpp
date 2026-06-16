#include "rack_oriented_solver.h"

#include "utils.h"
#include "validation.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <vector>

#define UINT128_CSTRING_SLOTS 15
#define MAX_RACK_TILES 7
#define RACK_SUBSET_KEY_CAPACITY 128

typedef struct {
    uint64_t keys[MAX_RACK_TILES + 1][RACK_SUBSET_KEY_CAPACITY];
    uint8_t counts[MAX_RACK_TILES + 1];
} RackSubsetKeys;

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

static RackSubsetKeys rack_subset_keys_for_rack(uint64_t sorted_rack, uint8_t rack_length)
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

    return subset_keys;
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
        // todo: when does this happen?
        return 0;
    }

    uint32_t start = entry->word_indices.start;
    for (size_t offset = 0; offset < entry->word_indices.count; ++offset) {
        uint32_t word_index = word_patterns->word_indices[(size_t)start + offset];

        const Row *row_with_just_proposed_word = word_index_start_row_table_get(word_start_rows, word_index, window->left);

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

// note that the `rack_cstring` parameter must be passed in already sorted and with 1 to 7 characters.
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
    RackSubsetKeys subset_keys = rack_subset_keys_for_rack(rack, rack_length);
    std::vector<RackWindowMatch> window_matches;
    window_matches.reserve(BOARD_SIZE * BOARD_SIZE);

    for (uint8_t rack_pattern_length = 1; rack_pattern_length <= rack_length; ++rack_pattern_length) {
        uint8_t rack_subset_count = subset_keys.counts[rack_pattern_length];

        window_matches.clear();

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

        for (size_t match_index = 0; match_index < window_matches.size(); ++match_index) {
            for (uint8_t rack_subset_index = 0; rack_subset_index < rack_subset_count; ++rack_subset_index) {
                uint64_t rack_subset = subset_keys.keys[rack_pattern_length][rack_subset_index];

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
