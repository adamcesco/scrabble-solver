#ifndef SOLVER_H
#define SOLVER_H

#include "word_pattern_table.h"

inline constexpr uint8_t MAX_RACK_TILES = 7;

struct DirectionalCounts {
    size_t across;
    size_t down;
};

// Count the current view and its transpose, restoring the prepared board on return.
// The low bytes of sorted_rack hold rack_length sorted A-Z tiles (up to seven).
DirectionalCounts count_moves(const WordTable &dictionary, const WordPatternTable &patterns,
                             Board &board, uint64_t sorted_rack, uint8_t rack_length);

#endif
