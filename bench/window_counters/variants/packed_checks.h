#ifndef BENCH_WINDOW_COUNTERS_VARIANTS_PACKED_CHECKS_H
#define BENCH_WINDOW_COUNTERS_VARIANTS_PACKED_CHECKS_H

#include "gap_layouts.h"
#include "solvers/packed/counter.h"

#include <type_traits>

namespace rack_solver::experiments {
enum class MaskPolicy { lazy, eager, stop_gap, stop_window };
enum class WindowCounting { all_gaps_per_word, constrained_per_word, bulk_unrestricted };

// Vary mask construction and candidate checks within the packed solver's group loop.
template <GapTraversal traversal, MaskPolicy policy, WindowCounting counting, typename Layout,
          GapEvaluation evaluation, auto order_gaps>
struct TrialChecks {
    using Masks = WindowMasks<policy == MaskPolicy::stop_gap || policy == MaskPolicy::stop_window,
                              counting != WindowCounting::all_gaps_per_word, Layout>;
    static constexpr bool eager = policy == MaskPolicy::eager;
    static constexpr GroupSearch on_impossible =
        policy == MaskPolicy::stop_window ? GroupSearch::stop_window : GroupSearch::keep_going;

    template <typename Checks>
    static void build(std::optional<Masks> &masks, Checks &checks, const MatchedWindow &window, uint16_t neighbors)
    {
        if constexpr (traversal == GapTraversal::scan) {
            masks.emplace(window, checks);
        } else if constexpr (traversal == GapTraversal::empty_bits) {
            masks.emplace(checks, window, UINT16_MAX);
        } else {
            masks.emplace(checks, window, neighbors);
        }
        if constexpr (order_gaps != nullptr) {
            order_gaps(*masks);
        }
        bool singleton = false;
        if constexpr (std::is_same_v<Layout, SingletonGaps>) {
            masks->gaps.prepare(masks->ruled_out, masks->gap_count);
            singleton = masks->gaps.required_letter != 0;
        }
        trace_mask_built(masks->ruled_out, masks->gap_count, singleton);
    }

    template <typename Words>
    static size_t count_words(const Words &words, const Masks &masks)
    {
        const bool bulk = counting == WindowCounting::bulk_unrestricted && !masks.gap_count;
        return bulk ? words.decide(true) : words.template check_words<evaluation>(masks);
    }
};

template <GapTraversal traversal = GapTraversal::scan, MaskPolicy policy = MaskPolicy::lazy,
          WindowCounting counting = WindowCounting::all_gaps_per_word, typename Layout = GapDescriptors,
          GapEvaluation evaluation = GapEvaluation::skip_unrestricted, auto order_gaps = nullptr,
          typename Words = GroupWords>
using WindowCounter = PackedWindowCounter<TrialChecks<traversal, policy, counting, Layout, evaluation, order_gaps>, Words>;

} // namespace rack_solver::experiments

#endif
