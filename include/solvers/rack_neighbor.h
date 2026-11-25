#ifndef INCLUDE_SOLVERS_RACK_NEIGHBOR_H
#define INCLUDE_SOLVERS_RACK_NEIGHBOR_H

#include "dictionary.h"
#include "solvers/shared.h"

// Count both directions on a prepared board using the uncached rack-neighbor counter.
// The rack contains rack_length sorted A-Z tiles in its low bytes.
DirectionalCounts rack_neighbor_both_directions(const WordTable &dictionary, const WordPatternTable &patterns,
                                              Board &board, uint64_t sorted_rack, uint8_t rack_length);

#endif
