#include "validation.h"

#include <stdio.h>
#include <stdint.h>
#include <stdio.h>

static inline uint16_t get_start_pos_mask_of_housting_perpendicular_word(uint16_t perpendicular_row_occupied, uint16_t new_word_perpendicular_occupied)
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

int validate_perpendicular_rows(const WordTable *dictionary, const uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1], const Board *new_board, const Row old_board_perpendicular_rows[BOARD_SIZE], uint8_t word_start, uint8_t word_length, uint8_t row_that_houses_new_word)
{
    uint16_t new_word_perpendicular_occupied = 1u << row_that_houses_new_word;
    
    for (uint8_t col_index = word_start; col_index < word_start + word_length; ++col_index) {
        if ((old_board_perpendicular_rows[col_index].occupiedMask & new_word_perpendicular_occupied) == old_board_perpendicular_rows[col_index].occupiedMask) {
            continue;
        }
        const Row *perpendicular_row = &new_board->perpendicularRows[col_index];
        const uint16_t start_mask = get_start_pos_mask_of_housting_perpendicular_word(perpendicular_row->occupiedMask, new_word_perpendicular_occupied);
        const char * tile_c_string = ((const char *)&perpendicular_row->tiles) + config_to_start_positions[start_mask][1];
        if (tile_c_string[1] != '\0' && !word_table_contains(dictionary, tile_c_string)) {
            return 0;
        }
    }

    return 1;
}
