#ifndef VALIDATION_H
#define VALIDATION_H

#include "board.h"
#include "dictionary.h"

// Caller must guarantee row matches every already-occupied board cell in the
// proposed span; occupied intersections are treated as unchanged and skipped.
int place_word_onto_perpendicular_rows_and_validate(
    const WordTable *dictionary,
    const uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1],
    const Row old_board_perpendicular_rows[BOARD_SIZE],
    Board *board,
    const Row *row,
    uint8_t row_index,
    uint8_t word_start,
    uint8_t word_length
);

#endif
