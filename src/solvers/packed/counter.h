#ifndef SRC_SOLVERS_PACKED_COUNTER_H
#define SRC_SOLVERS_PACKED_COUNTER_H

#include "../shared/group_words.h"

#include <optional>

namespace rack_solver {
struct PackedChecks {
    // Keep every gap and finish the scan even when a perpendicular-check set is empty.
    using Masks = WindowMasks</*stop_at_impossible=*/false, /*constrained_only=*/false>;
    static constexpr bool eager = false;
    static constexpr GroupSearch on_impossible = GroupSearch::keep_going;

    template <typename Checks>
    static void build(std::optional<Masks> &masks, Checks &checks, const MatchedWindow &window, uint16_t)
    {
        masks.emplace(window, checks);
        trace_mask_built(masks->ruled_out, masks->gap_count);
    }

    template <typename Words>
    static size_t count_words(const Words &words, const Masks &masks)
    {
        return words.template check_words<GapEvaluation::skip_unrestricted>(masks);
    }
};

template <typename Validation = PackedChecks, typename Words = GroupWords>
struct PackedWindowCounter {
    const WordTable &dictionary;
    const WordPatternTable &patterns;

    PackedWindowCounter(const WordTable &dictionary, const WordPatternTable &patterns)
        : dictionary(dictionary), patterns(patterns) {}

    PackedWindowCounter(const WordTable &dictionary, const WordPatternTable &patterns, uint64_t, uint8_t)
        : PackedWindowCounter(dictionary, patterns) {}

    template <typename Checks, typename VisitGroups>
    size_t operator()(Checks &checks, uint16_t neighbors, const MatchedWindow &window,
                      uint8_t, VisitGroups visit_groups) const
    {
        size_t count = 0;
        std::optional<typename Validation::Masks> masks;
        if constexpr (Validation::eager) {
            Validation::build(masks, checks, window, neighbors);
        }
        visit_groups([&](const WordPatternAnagramGroup &group) {
            if (!Validation::eager && !masks) {
                Validation::build(masks, checks, window, neighbors);
            }
            const Words words{dictionary, patterns.word_indices.data() + group.word_indices.start,
                              group.word_indices.count, checks.row, window.start};
            if (masks->ruled_out) {
                words.decide(false);
                return Validation::on_impossible;
            }
            count += Validation::count_words(words, *masks);
            return GroupSearch::keep_going;
        });
        return count;
    }
};

using PackedCounter = PackedWindowCounter<>;
} // namespace rack_solver

#endif
