#ifndef BENCH_WINDOW_COUNTERS_VARIANTS_SINGLETON_WORDS_H
#define BENCH_WINDOW_COUNTERS_VARIANTS_SINGLETON_WORDS_H

#include "solvers/shared/group_words.h"

namespace rack_solver::experiments {
enum class SingletonTest { mask, byte };

template <SingletonTest test>
struct SingletonWords : GroupWords {
    using GroupWords::GroupWords;

    template <GapEvaluation evaluation, typename Layout>
    size_t check_words(const GapChecks<Layout> &checks) const
    {
        const char required_letter = checks.gaps.required_letter;
        if (!required_letter) {
            return GroupWords::check_words<evaluation>(checks);
        }
        if constexpr (test == SingletonTest::mask) {
            return count_if([&](const char *word) { return checks.template accepts<evaluation>(word, 1); });
        } else {
            const uint8_t offset = checks.gaps.offset_at(0);
            return count_if([&](const char *word) {
                const bool passes = word[offset] == required_letter;
                trace_gap_check(0, !passes);
                return passes;
            });
        }
    }
};
} // namespace rack_solver::experiments

#endif
