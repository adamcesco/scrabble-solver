#include "validation.h"

#define TILE_BITS ROW_TILE_BITS
#define TILE_MASK ROW_TILE_MASK

static int is_word_at_pos_in_perpendicular_row_valid(
    const WordTable *dictionary,
    Row perpendicular_row,
    uint16_t starting_pos
)
{
    return word_table_contains(dictionary, ((char *)&perpendicular_row) + starting_pos);
}

int validate_perpendicular_rows(const WordTable *dictionary, const uint16_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1], Board board, uint16_t word_start, uint16_t word_length)
{
    for (uint16_t col_index = word_start; col_index < word_start + word_length; ++col_index) {
        Row perpendicular_row = board.perpendicularRows[col_index];
        uint16_t start_mask = make_word_start_mask(perpendicular_row.occupiedMask);
        for (uint16_t starting_pos_index = 1; starting_pos_index <= config_to_start_positions[start_mask][0]; ++starting_pos_index) {
            uint16_t starting_pos = config_to_start_positions[start_mask][starting_pos_index];
            if (!is_word_at_pos_in_perpendicular_row_valid(dictionary, perpendicular_row, starting_pos)) {
                return 0;
            }
        }
    }

    return 1;
}
