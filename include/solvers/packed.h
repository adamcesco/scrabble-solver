#ifndef INCLUDE_SOLVERS_PACKED_H
#define INCLUDE_SOLVERS_PACKED_H

#include "dictionary.h"
#include "solvers/shared.h"

// Count both directions on a prepared board. The low rack_length bytes hold sorted A-Z tiles.
DirectionalCounts packed_both_directions(const WordTable &dictionary, const WordPatternTable &patterns,
                                         Board &board, uint64_t sorted_rack, uint8_t rack_length);

#endif
