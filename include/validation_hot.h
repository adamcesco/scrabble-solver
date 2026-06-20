#ifndef VALIDATION_HOT_H
#define VALIDATION_HOT_H

#include "board.h"
#include "dictionary.h"

#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#define HOT_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define HOT_ALWAYS_INLINE inline
#endif

static HOT_ALWAYS_INLINE uint16_t hot_start_pos_mask_of_housing_perpendicular_word(
    uint16_t perpendicular_row_occupied,
    uint16_t new_word_perpendicular_occupied
)
{
    if ((perpendicular_row_occupied & new_word_perpendicular_occupied) == 0) {
        return 0;
    }

    uint16_t z = (uint16_t)(~perpendicular_row_occupied & (new_word_perpendicular_occupied - 1u));
    z |= z >> 1;
    z |= z >> 2;
    z |= z >> 4;
    z |= z >> 8;

    return (uint16_t)(z + 1u);
}

static HOT_ALWAYS_INLINE int hot_validate_perpendicular_row(
    const WordTable *dictionary,
    const Row *perpendicular_row,
    uint16_t old_occupied_mask,
    uint8_t row_that_houses_new_word
)
{
    uint16_t new_tile_mask = (uint16_t)(1u << row_that_houses_new_word);

    if ((old_occupied_mask & new_tile_mask) == old_occupied_mask) {
        return 1;
    }

    const uint16_t start_mask = hot_start_pos_mask_of_housing_perpendicular_word(
        perpendicular_row->occupiedMask,
        new_tile_mask
    );
    const char *tile_c_string = ((const char *)&perpendicular_row->tiles) + __builtin_ctz(start_mask);

    if (tile_c_string[1] != '\0' && !dictionary->contains(tile_c_string)) {
        return 0;
    }

    return 1;
}

static HOT_ALWAYS_INLINE void hot_place_perpendicular_tile(
    Board *board,
    const Row *row,
    uint8_t row_index,
    uint8_t col_index
)
{
    const int perpendicular_shift = row_tile_shift_at_col(row_index);
    const RowTiles perpendicular_tile_mask = row_tile_mask_at_col(row_index);
    const uint16_t perpendicular_row_occupied_mask = (uint16_t)(1u << row_index);
    const int row_shift = row_tile_shift_at_col(col_index);
    const RowTiles tile = (row->tiles & row_tile_mask_at_col(col_index)) >> row_shift;
    Row *perpendicular_row = &board->perpendicularRows[col_index];

    perpendicular_row->tiles = (perpendicular_row->tiles & ~perpendicular_tile_mask) | (tile << perpendicular_shift);
    perpendicular_row->careMask |= perpendicular_tile_mask;
    perpendicular_row->occupiedMask |= perpendicular_row_occupied_mask;
}

static HOT_ALWAYS_INLINE int hot_place_word_onto_perpendicular_rows_and_validate_with_touched_mask(
    const WordTable *dictionary,
    const Row old_board_perpendicular_rows[BOARD_SIZE],
    Board *board,
    const Row *row,
    uint8_t row_index,
    uint8_t word_start,
    uint8_t word_length,
    uint16_t *touched_columns
)
{
    uint16_t touched = 0;

    for (uint8_t col_index = word_start; col_index < word_start + word_length; ++col_index) {
        const uint16_t row_mask = (uint16_t)(1u << row_index);

        if ((old_board_perpendicular_rows[col_index].occupiedMask & row_mask) != 0) {
            continue;
        }

        hot_place_perpendicular_tile(board, row, row_index, col_index);
        touched |= (uint16_t)(1u << col_index);

        if (!hot_validate_perpendicular_row(
                dictionary,
                &board->perpendicularRows[col_index],
                old_board_perpendicular_rows[col_index].occupiedMask,
                row_index
            )) {
            *touched_columns = touched;
            return 0;
        }
    }

    *touched_columns = touched;
    return 1;
}

#undef HOT_ALWAYS_INLINE

#endif
