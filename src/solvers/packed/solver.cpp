#include "solvers/packed.h"
#include "counter.h"

DirectionalCounts packed_both_directions(const WordTable &dictionary, const WordPatternTable &patterns,
                                         Board &board, uint64_t sorted_rack, uint8_t rack_length)
{
    return rack_solver::solve_rack(patterns, board, sorted_rack, rack_length, [&] {
        return rack_solver::PackedCounter(dictionary, patterns);
    });
}
