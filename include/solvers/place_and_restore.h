#ifndef INCLUDE_SOLVERS_PLACE_AND_RESTORE_H
#define INCLUDE_SOLVERS_PLACE_AND_RESTORE_H

#include "dictionary.h"
#include "solvers/shared.h"

// Count both directions on an optionally prepared board. The low rack_length bytes hold sorted A-Z tiles.
DirectionalCounts place_and_restore_both_directions(const WordTable &dictionary, const WordPatternTable &patterns,
                                                    const WordIndexStartRowTable &word_start_rows, Board &board,
                                                    uint64_t sorted_rack, uint8_t rack_length);

#endif
