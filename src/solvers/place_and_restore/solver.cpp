#include "solvers/place_and_restore.h"
#include "counter.h"

DirectionalCounts place_and_restore_both_directions(const WordTable &dictionary, const WordPatternTable &patterns,
                                                    const WordIndexStartRowTable &word_start_rows, Board &board,
                                                    uint64_t sorted_rack, uint8_t rack_length)
{
    const auto keys = rack_subset_keys_for_rack(sorted_rack, rack_length);
    return count_both_views(board, [&] {
        const rack_solver::PlaceAndRestoreCounter counter(dictionary, patterns, word_start_rows, board);
        return rack_solver::count_rack_windows(&patterns, &keys, &board, rack_length, counter);
    });
}
