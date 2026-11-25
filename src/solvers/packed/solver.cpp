#include "solvers/packed.h"
#include "counter.h"

DirectionalCounts packed_both_directions(const WordTable &dictionary, const WordPatternTable &patterns,
                                         Board &board, uint64_t sorted_rack, uint8_t rack_length)
{
    const auto keys = rack_subset_keys_for_rack(sorted_rack, rack_length);
    const rack_solver::PackedCounter counter(dictionary, patterns);
    return count_both_views(board, [&] {
        return rack_solver::count_rack_windows(&patterns, &keys, &board, rack_length, counter);
    });
}
