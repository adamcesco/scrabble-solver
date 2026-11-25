#ifndef BENCH_WINDOW_COUNTERS_VARIANTS_CACHED_ROW_CHECK_SETS_H
#define BENCH_WINDOW_COUNTERS_VARIANTS_CACHED_ROW_CHECK_SETS_H

#include "solvers/shared/rack_windows.h"

namespace rack_solver::experiments {
struct CachedRowCheckSets : RowCheckSets {
    using RowCheckSets::RowCheckSets;
    uint16_t valid = 0;
    uint32_t letters[BOARD_SIZE];

    uint32_t get(uint8_t col)
    {
        const uint16_t bit = bit_at_u16(col);
        if (valid & bit) {
#ifdef SCRABBLE_TRACE_PLACEMENTS
            trace_cache_hit();
#endif
            return letters[col];
        }
        letters[col] = RowCheckSets::get(col);
        valid |= bit;
        return letters[col];
    }
};
} // namespace rack_solver::experiments

#endif
