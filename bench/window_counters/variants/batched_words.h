#ifndef BENCH_WINDOW_COUNTERS_VARIANTS_BATCHED_WORDS_H
#define BENCH_WINDOW_COUNTERS_VARIANTS_BATCHED_WORDS_H

#include "solvers/shared/group_words.h"

namespace rack_solver::experiments {
enum class BatchHint { none, interleave, prefetch };

template <size_t width, BatchHint hint = BatchHint::none>
struct BatchedWords : GroupWords {
    using GroupWords::GroupWords;

    template <GapEvaluation evaluation, typename Layout>
    size_t check_words(const GapChecks<Layout> &checks) const
    {
        auto accepts = [&](const char *word) {
            return checks.template accepts<evaluation>(word, checks.gap_count);
        };
        auto check_batch = [&](size_t i) {
            const char *words[width];
            for (size_t j = 0; j < width; ++j) {
                words[j] = dictionary.words[ids[i + j]];
            }
            if constexpr (hint == BatchHint::prefetch) {
                if (i + width < size) {
                    __builtin_prefetch(dictionary.words[ids[i + width]]);
                }
            }
            size_t count = 0;
            for (size_t j = 0; j < width; ++j) {
                count += check_word(i + j, words[j], accepts);
            }
            return count;
        };
        const size_t end = size - size % width;
        size_t count = 0;
        if constexpr (hint == BatchHint::interleave) {
#ifdef __clang__
#pragma clang loop interleave_count(2)
#endif
            for (size_t i = 0; i < end; i += width) {
                count += check_batch(i);
            }
        } else {
            for (size_t i = 0; i < end; i += width) {
                count += check_batch(i);
            }
        }
        return count + count_if(accepts, end);
    }
};
} // namespace rack_solver::experiments

#endif
