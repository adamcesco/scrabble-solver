#include "solvers/place_and_restore.h"
#include "counter.h"

DirectionalCounts place_and_restore_both_directions(const WordTable &dictionary, const WordPatternTable &patterns,
                                                    const WordIndexStartRowTable &word_start_rows, Board &board,
                                                    uint64_t sorted_rack, uint8_t rack_length)
{
    return rack_solver::solve_rack(patterns, board, sorted_rack, rack_length, [&](Board &view) {
        return rack_solver::PlaceAndRestoreCounter(dictionary, patterns, word_start_rows, view);
    });
}
