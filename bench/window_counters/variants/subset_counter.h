#ifndef BENCH_WINDOW_COUNTERS_VARIANTS_SUBSET_COUNTER_H
#define BENCH_WINDOW_COUNTERS_VARIANTS_SUBSET_COUNTER_H

#include "solvers/rack_neighbor/counter.h"
#include "window_variants.h"

namespace rack_solver::experiments {
#ifndef SCRABBLE_TRACE_PLACEMENTS
inline void trace_subset(unsigned, benchmark::SubsetRejection) {}
#endif

enum class SubsetPolicy { alphabet, capacity, capacity_multi };

inline bool has_tile_capacity(const GapChecks<> &checks, PatternBytes key, uint8_t tile_count)
{
    for (uint8_t i = 1; i < checks.gap_count; ++i) {
        const uint32_t letters = checks.gaps.letters_at(i);
        unsigned needed = 1;
        for (uint8_t j = 0; j < i; ++j) {
            needed += checks.gaps.letters_at(j) == letters;
        }
        if (needed > 1) {
            unsigned available = 0;
            for (uint8_t tile = 0; tile < tile_count; ++tile) {
                const auto letter = get_byte_128(key, tile, BOARD_SIZE);
                available += (letters >> (letter - 'A')) & 1u;
            }
            if (available < needed) {
                return false;
            }
        }
    }
    return true;
}

template <SubsetPolicy policy>
size_t count_subset_words(const GroupWords &words, const GapChecks<> &window, PatternBytes key, uint8_t tile_count)
{
    if constexpr (policy == SubsetPolicy::capacity_multi) {
        if (words.size == 1) {
            return words.check_words(window);
        }
    }
    uint32_t alphabet = 0;
    for (uint8_t i = 0; i < tile_count; ++i) {
        alphabet |= 1u << (get_byte_128(key, i, BOARD_SIZE) - 'A');
    }

    GapChecks<> subset;
    unsigned removed = 0;
    for (uint8_t i = 0; i < window.gap_count; ++i) {
        const uint32_t eligible = window.gaps.letters_at(i) & alphabet;
        if (!eligible) {
            trace_subset(removed, benchmark::SubsetRejection::disjoint);
            return words.decide(false);
        }
        if (eligible == alphabet) {
            ++removed;
        } else {
            subset.append(window.gaps.offset_at(i), eligible);
        }
    }
    if constexpr (policy != SubsetPolicy::alphabet) {
        if (!has_tile_capacity(subset, key, tile_count)) {
            trace_subset(removed, benchmark::SubsetRejection::capacity);
            return words.decide(false);
        }
    }
    trace_subset(removed, benchmark::SubsetRejection::none);
    return subset.gap_count ? words.check_words(subset) : words.decide(true);
}

template <SubsetPolicy policy>
using SubsetCounter = RackCounter<GapTraversal::scan, GapEvaluation::skip_unrestricted, count_subset_words<policy>>;
} // namespace rack_solver::experiments

#endif
