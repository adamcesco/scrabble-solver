#include "validation.h"

#include <stdint.h>

static inline uint16_t get_start_pos_mask_of_housing_perpendicular_word(uint16_t perpendicular_row_occupied, uint16_t new_word_perpendicular_occupied)
{
    // Assumes bit is a single-bit mask.
    // If selected bit is not inside a 1-group, return 0.
    if ((perpendicular_row_occupied & new_word_perpendicular_occupied) == 0)
        return 0;

    // Find zeros below the selected bit.
    uint16_t z = (uint16_t)(~perpendicular_row_occupied & (new_word_perpendicular_occupied - 1u));

    // Smear the highest zero-below bit downward.
    z |= z >> 1;
    z |= z >> 2;
    z |= z >> 4;
    z |= z >> 8;

    // bottom is the least-significant 1 bit of the containing group.
    return z + 1u;
}

static inline int perpendicular_row_needs_dictionary_check(uint16_t old_occupied_mask, uint16_t new_tile_mask)
{
    return (old_occupied_mask & new_tile_mask) != old_occupied_mask;
}

static inline int validate_perpendicular_row(
    const WordTable *dictionary,
    const uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1],
    const Row *perpendicular_row,
    uint16_t old_occupied_mask,
    uint8_t row_that_houses_new_word
)
{
    uint16_t new_tile_mask = (uint16_t)(1u << row_that_houses_new_word);
    
    if (!perpendicular_row_needs_dictionary_check(old_occupied_mask, new_tile_mask)) {
        return 1;
    }
    
    const uint16_t start_mask = get_start_pos_mask_of_housing_perpendicular_word(perpendicular_row->occupiedMask, new_tile_mask);
    const char * tile_c_string = ((const char *)&perpendicular_row->tiles) + config_to_start_positions[start_mask][1];

    if (tile_c_string[1] != '\0' && !word_table_contains(dictionary, tile_c_string)) {
        return 0;
    }

    return 1;
}

static inline void place_perpendicular_tile(Board *board, const Row *row, uint8_t row_index, uint8_t col_index)
{
    int perpendicular_shift = row_tile_shift_at_col(row_index);
    RowTiles perpendicular_tile_mask = row_tile_mask_at_col(row_index);
    uint16_t perpendicular_row_occupied_mask = (uint16_t)(1u << row_index);
    int row_shift = row_tile_shift_at_col(col_index);
    RowTiles tile = (row->tiles & row_tile_mask_at_col(col_index)) >> row_shift;
    Row *perpendicular_row = &board->perpendicularRows[col_index];

    perpendicular_row->tiles = (perpendicular_row->tiles & ~perpendicular_tile_mask) | (tile << perpendicular_shift);
    perpendicular_row->careMask |= perpendicular_tile_mask;
    perpendicular_row->occupiedMask |= perpendicular_row_occupied_mask;
}

int place_word_onto_perpendicular_rows_and_validate(
    const WordTable *dictionary,
    const uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1],
    const Row old_board_perpendicular_rows[BOARD_SIZE],
    Board *board,
    const Row *row,
    uint8_t row_index,
    uint8_t word_start,
    uint8_t word_length
)
{
    for (uint8_t col_index = word_start; col_index < word_start + word_length; ++col_index) {
        place_perpendicular_tile(board, row, row_index, col_index);

        if (!validate_perpendicular_row(
                dictionary,
                config_to_start_positions,
                &board->perpendicularRows[col_index],
                old_board_perpendicular_rows[col_index].occupiedMask,
                row_index
            )) {
            return 0;
        }
    }

    return 1;
}
