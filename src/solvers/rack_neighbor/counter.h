#ifndef SRC_SOLVERS_RACK_NEIGHBOR_COUNTER_H
#define SRC_SOLVERS_RACK_NEIGHBOR_COUNTER_H

#include "../shared/group_words.h"
#include "utils.h"

#include <optional>

namespace rack_solver {
struct RackAlphabet {
    uint32_t letters = 0;
    uint32_t eligible(uint32_t allowed) const { return allowed & letters; }
    bool unrestricted(uint32_t eligible) const { return eligible == letters; }
};

template <GapTraversal traversal, GapEvaluation evaluation = GapEvaluation::skip_unrestricted,
          auto count_subset = nullptr>
struct RackCounter {
    static_assert(traversal == GapTraversal::scan || traversal == GapTraversal::neighbor_bits);
    const WordTable &dictionary;
    const WordPatternTable &patterns;
    RackAlphabet alphabet;

    RackCounter(const WordTable &dictionary, const WordPatternTable &patterns, uint64_t rack, uint8_t rack_length)
        : dictionary(dictionary), patterns(patterns)
    {
        for (uint8_t i = 0; i < rack_length; ++i) {
            alphabet.letters |= 1u << (get_byte_64(rack, i) - 'A');
        }
    }

    template <typename Checks, typename VisitGroups>
    size_t operator()(Checks &checks, uint16_t neighbors, const MatchedWindow &window,
                      uint8_t empty_count, VisitGroups visit_groups) const
    {
        size_t count = 0;
        std::optional<WindowMasks</*stop_at_impossible=*/true, /*constrained_only=*/true>> masks;
        visit_groups([&](const WordPatternAnagramGroup &group) {
            if (!masks) {
                if constexpr (traversal == GapTraversal::neighbor_bits) {
                    masks.emplace(checks, window, neighbors, alphabet);
                } else {
                    masks.emplace(window, checks, alphabet);
                }
                trace_mask_built(masks->ruled_out, masks->gap_count);
            }
            const GroupWords words{dictionary, patterns.word_indices.data() + group.word_indices.start,
                                   group.word_indices.count, checks.row, window.start};
            if (masks->ruled_out) {
                words.decide(false);
                return GroupSearch::stop_window;
            }
            if (!masks->gap_count) {
                count += words.decide(true);
            } else if constexpr (count_subset != nullptr) {
                count += count_subset(words, *masks, group.rack_letters, empty_count);
            } else {
                count += words.check_words<evaluation>(*masks);
            }
            return GroupSearch::keep_going;
        });
        return count;
    }
};

using RackNeighborCounter = RackCounter<GapTraversal::neighbor_bits>;
} // namespace rack_solver

#endif
