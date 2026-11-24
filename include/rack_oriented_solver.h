#ifndef RACK_ORIENTED_SOLVER_H
#define RACK_ORIENTED_SOLVER_H

#include "board.h"
#include "dictionary.h"

#include <stddef.h>
#include <stdint.h>

#define MAX_RACK_TILES 7
#define RACK_SUBSET_KEY_CAPACITY 128

typedef struct {
    uint64_t keys[MAX_RACK_TILES + 1][RACK_SUBSET_KEY_CAPACITY];
    uint8_t counts[MAX_RACK_TILES + 1];
} RackSubsetKeys;

RackSubsetKeys rack_subset_keys_for_rack(uint64_t sorted_rack, uint8_t rack_length);

// Count placements in the active view of a board prepared with this dictionary.
size_t rack_oriented_full_board_solver(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const RackSubsetKeys *rack_subset_keys,
    const Board *board,
    uint8_t rack_length
);

struct DirectionalCounts {
    size_t across;
    size_t down;
};

// Solve both directions on the same prepared position and restore the initial view.
DirectionalCounts rack_oriented_both_directions(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const RackSubsetKeys *rack_subset_keys,
    Board *board,
    uint8_t rack_length
);

// Original place-and-restore validator, retained for comparison on unprepared boards.
size_t rack_oriented_full_board_solver(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const WordIndexStartRowTable *word_start_rows,
    RackSubsetKeys *rack_subset_keys,
    Board *board,
    uint8_t rack_length
);

#endif
