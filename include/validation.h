#ifndef VALIDATION_H
#define VALIDATION_H

#include "board.h"
#include "dictionary.h"

int validate_perpendicular_rows(
    const WordTable *dictionary,
    const uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1],
    const Board *new_board,
    const Row old_board_perpendicular_rows[BOARD_SIZE],
    uint8_t word_start,
    uint8_t word_length,
    uint8_t row_that_houses_new_word
);

#endif
