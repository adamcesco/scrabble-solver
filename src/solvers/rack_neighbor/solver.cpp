#include "solvers/rack_neighbor.h"
#include "counter.h"

DirectionalCounts rack_neighbor_both_directions(const WordTable &dictionary, const WordPatternTable &patterns,
                                              Board &board, uint64_t sorted_rack, uint8_t rack_length)
{
    return rack_solver::solve_rack(patterns, board, sorted_rack, rack_length, [&] {
        return rack_solver::RackNeighborCounter(dictionary, patterns, sorted_rack, rack_length);
    });
}
