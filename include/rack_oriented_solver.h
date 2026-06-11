#ifndef RACK_ORIENTED_SOLVER_H
#define RACK_ORIENTED_SOLVER_H

#include "board.h"
#include "dictionary.h"

#include <stdint.h>

int rack_oriented_solver(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const char *rack,
    const uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1],
    Board *board
);

#endif
