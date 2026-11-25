#ifndef BENCH_WINDOW_COUNTERS_VARIANTS_SIMD_WORDS_H
#define BENCH_WINDOW_COUNTERS_VARIANTS_SIMD_WORDS_H

#include "solvers/shared/group_words.h"

#ifdef __AVX2__
#include <immintrin.h>

namespace rack_solver::experiments {
struct SimdWords : GroupWords {
    using GroupWords::GroupWords;

    template <GapEvaluation evaluation, typename Layout>
    size_t check_words(const GapChecks<Layout> &checks) const
    {
        static_assert(evaluation == GapEvaluation::accumulate && MAX_RACK_TILES < 8);
        if (checks.gap_count < 4) {
            return GroupWords::check_words<evaluation>(checks);
        }
        const auto &gaps = checks.gaps;
        const __m128i masks = _mm_setr_epi32(gaps.letters_at(0), gaps.letters_at(1),
                                             gaps.letters_at(2), gaps.letters_at(3));
        return count_if([&](const char *word) {
            // Each lane reads one byte, even when its gap is the word's final character.
            const __m128i characters = _mm_setr_epi32(word[gaps.offset_at(0)], word[gaps.offset_at(1)],
                                                      word[gaps.offset_at(2)], word[gaps.offset_at(3)]);
            // Shift the permitted-letter bit to the sign bit of its lane.
            const __m128i shifts = _mm_sub_epi32(_mm_set1_epi32('A' + 31), characters);
            const int passing = _mm_movemask_ps(_mm_castsi128_ps(_mm_sllv_epi32(masks, shifts)));
#ifdef SCRABBLE_TRACE_PLACEMENTS
            bool preceding_lanes_pass = true;
            for (uint8_t i = 0; i < 4; ++i) {
                const bool passes = passing & (1 << i);
                trace_gap_check(i, preceding_lanes_pass && !passes);
                preceding_lanes_pass &= passes;
            }
#endif
            bool accepted = passing == 15;
            for (uint8_t i = 4; i < MAX_RACK_TILES && i < checks.gap_count; ++i) {
                accepted &= checks.template check_gap<evaluation>(word, i, accepted);
            }
            return accepted;
        });
    }
};
} // namespace rack_solver::experiments

#endif

#endif
