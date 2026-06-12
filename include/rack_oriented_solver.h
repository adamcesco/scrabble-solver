#ifndef RACK_ORIENTED_SOLVER_H
#define RACK_ORIENTED_SOLVER_H

#include "board.h"
#include "dictionary.h"

#include <stdint.h>

int rack_oriented_solver(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const WordIndexStartRowTable *word_start_rows,
    Board *board,
    const char *rack_cstring
);

#endif
