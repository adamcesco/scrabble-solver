#ifndef VALIDATION_H
#define VALIDATION_H

#include "board.h"
#include "dictionary.h"

int validate_perpendicular_rows(
    const WordTable *dictionary,
    const uint16_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1],
    Board board,
    uint16_t word_start,
    uint16_t word_length
);

#endif
