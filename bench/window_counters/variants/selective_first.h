#ifndef BENCH_WINDOW_COUNTERS_VARIANTS_SELECTIVE_FIRST_H
#define BENCH_WINDOW_COUNTERS_VARIANTS_SELECTIVE_FIRST_H

#include "solvers/shared/window_masks.h"

namespace rack_solver::experiments {
inline void order_by_selectivity(GapChecks<GapDescriptors> &checks)
{
    const uint8_t count = checks.gap_count;
    auto &entries = checks.gaps.entries;
    if (count < 2) {
        return;
    }
    // Stable insertion sort of at most seven gaps, counting each mask's letters once.
    unsigned char sizes[MAX_RACK_TILES];
    sizes[0] = __builtin_popcount(entries[0].letters);
    for (uint8_t i = 1; i < count; ++i) {
        const auto gap = entries[i];
        const auto size = __builtin_popcount(gap.letters);
        uint8_t j = i;
        while (j > 0 && sizes[j - 1] > size) {
            entries[j] = entries[j - 1];
            sizes[j] = sizes[j - 1];
            --j;
        }
        entries[j] = gap;
        sizes[j] = size;
    }
}
} // namespace rack_solver::experiments

#endif
