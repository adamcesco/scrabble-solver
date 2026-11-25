#ifndef BENCH_WINDOW_COUNTERS_VARIANTS_FIXED_GAP_WORDS_H
#define BENCH_WINDOW_COUNTERS_VARIANTS_FIXED_GAP_WORDS_H

#include "solvers/shared/group_words.h"

namespace rack_solver::experiments {
struct FixedGapWords : GroupWords {
    using GroupWords::GroupWords;

    template <GapEvaluation evaluation, typename Layout>
    size_t check_words(const GapChecks<Layout> &checks) const
    {
        switch (checks.gap_count) {
            case 1: return count_if([&](const char *word) { return checks.template accepts<evaluation>(word, 1); });
            case 2: return count_if([&](const char *word) { return checks.template accepts<evaluation>(word, 2); });
        }
        return GroupWords::check_words<evaluation>(checks);
    }
};
} // namespace rack_solver::experiments

#endif
