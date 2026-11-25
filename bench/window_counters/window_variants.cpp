#include "window_variants.h"
#include "solvers/packed/counter.h"
#include "solvers/rack_neighbor/counter.h"

#ifndef SCRABBLE_TUNING_PAIR
#include "variants/batched_words.h"
#include "variants/cached_row_check_sets.h"
#include "variants/fixed_gap_words.h"
#include "variants/gap_layouts.h"
#include "variants/one_gap_counter.h"
#include "variants/packed_checks.h"
#include "variants/selective_first.h"
#include "variants/simd_words.h"
#include "variants/singleton_words.h"
#include "variants/subset_counter.h"
#endif

#include <chrono>
#include <stdexcept>
#include <string>

namespace {
using namespace rack_solver;
#ifndef SCRABBLE_TUNING_PAIR
using namespace rack_solver::experiments;

template <size_t width, BatchHint hint = BatchHint::none>
using BatchCounter = PackedWindowCounter<PackedChecks, BatchedWords<width, hint>>;

template <WindowCounting counting>
using StoppingCounter = WindowCounter<GapTraversal::scan, MaskPolicy::stop_window, counting>;

template <GapEvaluation evaluation, auto order_gaps = nullptr, typename Words = GroupWords>
using PackedGapCounter = WindowCounter<GapTraversal::scan, MaskPolicy::lazy, WindowCounting::all_gaps_per_word,
                                       GapDescriptors, evaluation, order_gaps, Words>;

template <typename Layout>
using LayoutCounter = WindowCounter<GapTraversal::scan, MaskPolicy::lazy, WindowCounting::all_gaps_per_word, Layout>;

template <typename Words = GroupWords, typename Layout = GapDescriptors>
using ScalarCounter = WindowCounter<GapTraversal::scan, MaskPolicy::stop_window, WindowCounting::bulk_unrestricted,
                                    Layout, GapEvaluation::early_exit, nullptr, Words>;
#endif

template <typename Counter, typename Checks = RowCheckSets>
benchmark::WindowSolve measure_solve(const WordTable &words, const WordPatternTable &patterns,
                                     Board &board, const benchmark::Rack &rack)
{
#ifndef SCRABBLE_TRACE_PLACEMENTS
    using Clock = std::chrono::steady_clock;
    auto start = Clock::now();
#endif
    const auto keys = rack_subset_keys_for_rack(rack.tiles, rack.length);
    const Counter counter(words, patterns, rack.tiles, rack.length);
    auto counts = count_both_views(board, [&] {
        return count_rack_windows<Checks>(&patterns, &keys, &board, rack.length, counter);
    });
#ifdef SCRABBLE_TRACE_PLACEMENTS
    return {counts, 0};
#else
    auto end = Clock::now();
    return {counts, std::chrono::duration<double, std::micro>(end - start).count()};
#endif
}

constexpr benchmark::WindowVariant variants[] = {
    {"packed", measure_solve<PackedCounter>},
#ifndef SCRABBLE_TUNING_PAIR
    {"batch-2", measure_solve<BatchCounter<2>>},
    {"batch-4", measure_solve<BatchCounter<4>>},
#ifdef __clang__
    {"batch-2-interleave", measure_solve<BatchCounter<2, BatchHint::interleave>>},
    {"batch-4-interleave", measure_solve<BatchCounter<4, BatchHint::interleave>>},
#endif
    {"batch-2-prefetch", measure_solve<BatchCounter<2, BatchHint::prefetch>>},
    {"batch-4-prefetch", measure_solve<BatchCounter<4, BatchHint::prefetch>>},
    {"early-exit", measure_solve<PackedGapCounter<GapEvaluation::early_exit>>},
    {"all-checks", measure_solve<PackedGapCounter<GapEvaluation::accumulate>>},
#ifdef __AVX2__
    {"bounded-simd", measure_solve<PackedGapCounter<GapEvaluation::accumulate, nullptr, SimdWords>>},
#endif
    {"selective-first", measure_solve<PackedGapCounter<GapEvaluation::early_exit, order_by_selectivity>>},
    {"gap-u32", measure_solve<LayoutCounter<Uint32Gaps>>},
    {"gap-arrays", measure_solve<LayoutCounter<GapArrays>>},
    {"eager", measure_solve<WindowCounter<GapTraversal::scan, MaskPolicy::eager>>},
    {"stop-gap", measure_solve<WindowCounter<GapTraversal::scan, MaskPolicy::stop_gap>>},
    {"stop-window", measure_solve<StoppingCounter<WindowCounting::all_gaps_per_word>>},
    {"constrained-only", measure_solve<StoppingCounter<WindowCounting::constrained_per_word>>},
    {"all-letters", measure_solve<StoppingCounter<WindowCounting::bulk_unrestricted>>},
    {"scalar-general", measure_solve<ScalarCounter<>>},
    {"scalar-1-2", measure_solve<ScalarCounter<FixedGapWords>>},
    {"singleton-mask", measure_solve<ScalarCounter<SingletonWords<SingletonTest::mask>, SingletonGaps>>},
    {"singleton-byte", measure_solve<ScalarCounter<SingletonWords<SingletonTest::byte>, SingletonGaps>>},
#endif
    {"rack", measure_solve<RackCounter<GapTraversal::scan>>},
#ifndef SCRABBLE_TUNING_PAIR
    {"subset", measure_solve<SubsetCounter<SubsetPolicy::alphabet>>},
    {"subset-capacity", measure_solve<SubsetCounter<SubsetPolicy::capacity>>},
    {"subset-multi", measure_solve<SubsetCounter<SubsetPolicy::capacity_multi>>},
    {"row-cache", measure_solve<PackedCounter, CachedRowCheckSets>},
    {"one-gap", measure_solve<OneGapCounter>},
    {"empty-bits", measure_solve<WindowCounter<GapTraversal::empty_bits>>},
    {"neighbor-bits", measure_solve<WindowCounter<GapTraversal::neighbor_bits>>},
    {"neighbor-cache", measure_solve<WindowCounter<GapTraversal::neighbor_bits>, CachedRowCheckSets>},
    {"rack-cache", measure_solve<RackCounter<GapTraversal::scan>, CachedRowCheckSets>},
    {"rack-neighbor", measure_solve<RackNeighborCounter>},
    {"rack-neighbor-cache", measure_solve<RackNeighborCounter, CachedRowCheckSets>},
    {"rack-all-checks", measure_solve<RackCounter<GapTraversal::scan, GapEvaluation::accumulate>>},
#endif
};
} // namespace

const benchmark::WindowVariant &benchmark::window_variant(std::string_view name)
{
    for (const auto &variant : variants) {
        if (variant.name == name) {
            return variant;
        }
    }
    throw std::invalid_argument("unknown window counter: " + std::string(name));
}
