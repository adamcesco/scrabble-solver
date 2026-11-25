#ifndef BENCH_WINDOW_COUNTERS_VARIANTS_ONE_GAP_COUNTER_H
#define BENCH_WINDOW_COUNTERS_VARIANTS_ONE_GAP_COUNTER_H

#include "solvers/packed/counter.h"

namespace rack_solver::experiments {
struct OneGapCounter : PackedCounter {
    using PackedCounter::PackedCounter;

    template <typename VisitGroups>
    size_t operator()(RowCheckSets &checks, uint16_t neighbors, const MatchedWindow &window,
                      uint8_t empty_count, VisitGroups visit_groups) const
    {
        if (empty_count != 1) {
            return PackedCounter::operator()(checks, neighbors, window, empty_count, visit_groups);
        }
        std::optional<uint32_t> letters;
        size_t count = 0;
        visit_groups([&](const WordPatternAnagramGroup &group) {
            if (!letters) {
                const auto gap = window.empty_columns(checks.occupied());
                letters = checks.get(__builtin_ctz(gap));
            }
            const auto tile = get_byte_128(group.rack_letters, 0, BOARD_SIZE);
            const bool accepted = *letters & (1u << (tile - 'A'));
            if (accepted) {
                count += group.word_indices.count;
            }
            trace_placements(checks.row, window.start, patterns.word_indices.data() + group.word_indices.start,
                             group.word_indices.count, accepted);
            return GroupSearch::keep_going;
        });
        return count;
    }
};
} // namespace rack_solver::experiments

#endif
