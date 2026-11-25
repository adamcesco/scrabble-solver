#include "rack_oriented_solver.h"

#include "row_windows.h"
#include "utils.h"
#include "validation_hot.h"

#include <algorithm>
#include <optional>
#include <stdint.h>

#ifdef SCRABBLE_WINDOW_VARIANTS
#include "window_variants.h"
#include <chrono>
#include <string>
#endif

#ifdef SCRABBLE_TRACE_PLACEMENTS
#include "placement_trace.h"
#else
static inline void trace_placements(uint8_t, uint8_t, const uint32_t *, size_t, bool) {}
static inline void trace_window(uint8_t, uint8_t, uint8_t, uint8_t, bool) {}
static inline void trace_group(size_t, bool) {}
static inline void trace_skipped_subset_probe() {}
static inline void trace_skipped_merge_comparison() {}
static inline void trace_individual_check() {}
static inline void trace_mask_built(bool, unsigned) {}
#ifdef SCRABBLE_WINDOW_VARIANTS
static inline void trace_rack_gap_removed(bool) {}
static inline void trace_subset(unsigned, benchmark::SubsetRejection) {}
#endif
#endif

static inline int canonical_rack_subset_mask(uint8_t duplicate_follow_mask, uint8_t mask)
{
    return (duplicate_follow_mask & mask & (uint8_t)~(mask << 1u)) == 0;
}

static inline uint64_t compact_rack_subset(uint64_t sorted_rack, uint8_t mask)
{
    uint8_t k0 = (uint8_t)((mask >> 0u) & 1u);
    uint8_t k1 = (uint8_t)((mask >> 1u) & 1u);
    uint8_t k2 = (uint8_t)((mask >> 2u) & 1u);
    uint8_t k3 = (uint8_t)((mask >> 3u) & 1u);
    uint8_t k4 = (uint8_t)((mask >> 4u) & 1u);
    uint8_t k5 = (uint8_t)((mask >> 5u) & 1u);
    uint8_t k6 = (uint8_t)((mask >> 6u) & 1u);

    uint8_t m0 = (uint8_t)(0u - k0);
    uint8_t m1 = (uint8_t)(0u - k1);
    uint8_t m2 = (uint8_t)(0u - k2);
    uint8_t m3 = (uint8_t)(0u - k3);
    uint8_t m4 = (uint8_t)(0u - k4);
    uint8_t m5 = (uint8_t)(0u - k5);
    uint8_t m6 = (uint8_t)(0u - k6);

    unsigned p0 = 0;
    unsigned p1 = k0;
    unsigned p2 = k0 + k1;
    unsigned p3 = k0 + k1 + k2;
    unsigned p4 = k0 + k1 + k2 + k3;
    unsigned p5 = k0 + k1 + k2 + k3 + k4;
    unsigned p6 = k0 + k1 + k2 + k3 + k4 + k5;

    return put_byte_64((uint8_t)(get_byte_64(sorted_rack, 0) & m0), p0) |
        put_byte_64((uint8_t)(get_byte_64(sorted_rack, 1) & m1), p1) |
        put_byte_64((uint8_t)(get_byte_64(sorted_rack, 2) & m2), p2) |
        put_byte_64((uint8_t)(get_byte_64(sorted_rack, 3) & m3), p3) |
        put_byte_64((uint8_t)(get_byte_64(sorted_rack, 4) & m4), p4) |
        put_byte_64((uint8_t)(get_byte_64(sorted_rack, 5) & m5), p5) |
        put_byte_64((uint8_t)(get_byte_64(sorted_rack, 6) & m6), p6);
}

static inline uint8_t duplicate_follow_mask_for_rack(uint64_t sorted_rack, uint8_t rack_length)
{
    uint8_t duplicate_follow_mask = 0;

    for (uint8_t index = 1; index < rack_length; ++index) {
        if (get_byte_64(sorted_rack, index) == get_byte_64(sorted_rack, (uint8_t)(index - 1u))) {
            duplicate_follow_mask |= (uint8_t)(UINT8_C(1) << index);
        }
    }

    return duplicate_follow_mask;
}

RackSubsetKeys rack_subset_keys_for_rack(uint64_t sorted_rack, uint8_t rack_length)
{
    RackSubsetKeys subset_keys = {};
    const uint8_t duplicate_follow_mask = duplicate_follow_mask_for_rack(sorted_rack, rack_length);
    const uint8_t subset_count = (uint8_t)(UINT8_C(1) << rack_length);

    for (uint8_t mask = 1; mask < subset_count; ++mask) {
        if (!canonical_rack_subset_mask(duplicate_follow_mask, mask)) {
            continue;
        }

        uint8_t key_length = (uint8_t)__builtin_popcount(mask);
        uint8_t key_index = subset_keys.counts[key_length]++;

        subset_keys.keys[key_length][key_index] = compact_rack_subset(sorted_rack, mask);
    }

    for (uint8_t key_length = 1; key_length <= rack_length; ++key_length) {
        std::sort(
            subset_keys.keys[key_length],
            subset_keys.keys[key_length] + subset_keys.counts[key_length]
        );
    }

    return subset_keys;
}

static inline void restore_perpendicular_window(
    Board *board,
    const Board *copy_of_original_board,
    uint16_t touched_columns
)
{
    while (touched_columns != 0) {
        uint8_t col_index = (uint8_t)__builtin_ctz(touched_columns);
        board->perpendicularRows[col_index] = copy_of_original_board->perpendicularRows[col_index];
        touched_columns &= (uint16_t)(touched_columns - 1u);
    }
}

static inline int validate_rack_window_words(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const WordIndexStartRowTable *word_start_rows,
    Board *board,
    uint8_t row_index,
    uint8_t start_col,
    uint8_t word_length,
    const WordPatternAnagramGroup &entry,
    const Board *copy_of_original_board
)
{
    int count = 0;

    uint32_t start = entry.word_indices.start;
    for (size_t offset = 0; offset < entry.word_indices.count; ++offset) {
        const uint32_t &word_index = word_patterns->word_indices[(size_t)start + offset];

        const Row *row_with_just_proposed_word = word_start_rows->get_unchecked(word_index, start_col);

        uint16_t touched_columns = 0;
        bool accepted = hot_place_word_onto_perpendicular_rows_and_validate_with_touched_mask(
            dictionary,
            copy_of_original_board->perpendicularRows,
            board,
            row_with_just_proposed_word,
            row_index,
            start_col,
            word_length,
            &touched_columns
        );
        count += accepted;

        restore_perpendicular_window(board, copy_of_original_board, touched_columns);
        trace_placements(row_index, start_col, &word_index, 1, accepted);
    }

    return count;
}

namespace {
struct MatchedWindow {
    uint8_t start;
    uint8_t length;
    const WordPattern *match;

    uint16_t empty_columns(uint16_t occupied) const
    {
        return (low_bit_mask_u16(length) << start) & ~occupied;
    }
};

enum class GroupSearch { keep_going, stop_window };

template <typename Visitor>
static void visit_matching_anagrams(
    const WordPatternTable &word_patterns,
    uint8_t row, const MatchedWindow &window,
    const uint64_t *rack_subsets,
    uint8_t rack_subset_count,
    Visitor visit
)
{
    bool stopped = false;
    auto visit_group_and_check_stop = [&](const WordPatternAnagramGroup &group) {
        trace_group(group.word_indices.count, stopped);
        if (stopped) {
            const uint32_t *ids = word_patterns.word_indices.data() + group.word_indices.start;
            trace_placements(row, window.start, ids, group.word_indices.count, false);
        } else if (visit(group) == GroupSearch::stop_window) {
#ifdef SCRABBLE_TRACE_PLACEMENTS
            // Enumerate the remaining matches solely to qualify their rejections.
            stopped = true;
#else
            return true;
#endif
        }
        return false;
    };
    const WordPatternRange anagrams = window.match->anagrams;
    if (anagrams.count > (uint32_t)rack_subset_count * 4u) {
        for (uint8_t index = 0; index < rack_subset_count; ++index) {
            if (stopped) {
                trace_skipped_subset_probe();
            }
            const WordPatternAnagramGroup *group = word_patterns.get_anagram(*window.match, rack_subsets[index]);
            if (group != nullptr && visit_group_and_check_stop(*group)) {
                return;
            }
        }
        return;
    }

    const WordPatternAnagramGroup *groups = word_patterns.anagram_groups.data() + anagrams.start;
    uint32_t group_index = 0;
    uint8_t rack_subset_index = 0;

    while (group_index < anagrams.count && rack_subset_index < rack_subset_count) {
        if (stopped) {
            trace_skipped_merge_comparison();
        }
        const PatternBytes group_key = groups[group_index].rack_letters;
        const PatternBytes rack_key = rack_subsets[rack_subset_index];

        if (group_key < rack_key) {
            ++group_index;
        } else if (rack_key < group_key) {
            ++rack_subset_index;
        } else {
            if (visit_group_and_check_stop(groups[group_index])) {
                return;
            }
            ++group_index;
            ++rack_subset_index;
        }
    }
}

enum class GapTraversal { scan, empty_bits, neighbor_bits };

struct RowCheckSets {
    const Board &board;
    const uint8_t row;

    RowCheckSets(const Board &board, uint8_t row) : board(board), row(row) {}
    uint16_t occupied() const { return board.rows[row].occupiedMask; }
    uint32_t get(uint8_t col) const { return board.perpendicular_check_set(row, col); }
};

#ifdef SCRABBLE_WINDOW_VARIANTS
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
#endif

struct GapChecks {
    struct Gap {
        uint8_t offset;
        uint32_t letters;
    };
    Gap gaps[MAX_RACK_TILES];
    uint8_t gap_count = 0;

    bool accepts(const char *word) const
    {
        trace_individual_check();
        for (uint8_t i = 0; i < gap_count; ++i) {
            const Gap &gap = gaps[i];
            if (gap.letters != PerpendicularCheckTable::all_letters &&
                !(gap.letters & (1u << (word[gap.offset] - 'A')))) {
                return false;
            }
        }
        return true;
    }
};

struct GroupWords {
    const WordTable &dictionary;
    const uint32_t *ids;
    size_t size;
    uint8_t row, start;

    size_t decide(bool accepted) const
    {
        trace_placements(row, start, ids, size, accepted);
        return accepted ? size : 0;
    }

    size_t check_words(const GapChecks &checks) const
    {
        size_t count = 0;
        for (size_t i = 0; i < size; ++i) {
            const bool accepted = checks.accepts(dictionary.words[ids[i]]);
            count += accepted;
            trace_placements(row, start, ids + i, 1, accepted);
        }
        return count;
    }
};

template <bool stop_at_impossible>
struct WindowMasks : GapChecks {
    bool ruled_out = false;

    template <typename Checks>
    WindowMasks(Checks &checks, const MatchedWindow &window)
    {
        for (uint8_t offset = 0; offset < window.length; ++offset) {
            uint8_t col = window.start + offset;
            if (!(checks.occupied() & bit_at_u16(col))) {
                const uint32_t letters = checks.get(col);
                if (stop_at_impossible && !letters) {
                    ruled_out = true;
                    break;
                }
                add_gap(offset, letters);
            }
        }
    }

    template <typename Checks>
    WindowMasks(Checks &checks, const MatchedWindow &window, uint16_t eligible_columns)
    {
        uint16_t columns = window.empty_columns(checks.occupied()) & eligible_columns;
        while (columns) {
            const uint8_t col = __builtin_ctz(columns);
            add_gap(col - window.start, checks.get(col));
            columns &= columns - 1u;
        }
    }

    void add_gap(uint8_t offset, uint32_t letters)
    {
        gaps[gap_count++] = {offset, letters};
        ruled_out |= letters == 0;
    }
};

template <typename Checks = RowCheckSets, typename CountWindow>
size_t count_rack_windows(
    const WordPatternTable *word_patterns,
    const RackSubsetKeys *rack_subset_keys,
    const Board *board,
    uint8_t rack_length,
    CountWindow count_window
)
{
    size_t count = 0;
    for (uint8_t row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        // Each empty count has at most one complete window per start column:
        // its end must be just before the next empty square or at the board edge.
        MatchedWindow by_empty_count[MAX_RACK_TILES + 1][BOARD_SIZE];
        uint8_t counts[MAX_RACK_TILES + 1] = {};
        const uint16_t neighbors = visit_row_windows(
            board->rows, row_index, rack_length,
            [&](uint8_t start, const PatternRow &pattern, uint8_t empty_count) {
                const WordPattern *match = word_patterns->get(pattern);
                if (match != nullptr) {
                    by_empty_count[empty_count][counts[empty_count]++] = {start, pattern.length, match};
                } else {
                    trace_window(row_index, start, pattern.length, empty_count, false);
                }
            }
        );
        Checks checks(*board, row_index);
        for (uint8_t empty_count = 1; empty_count <= rack_length; ++empty_count) {
            for (uint8_t index = 0; index < counts[empty_count]; ++index) {
                const MatchedWindow &window = by_empty_count[empty_count][index];
                trace_window(row_index, window.start, window.length, empty_count, true);
                auto visit_groups = [&](auto visit) {
                    visit_matching_anagrams(
                        *word_patterns, row_index, window,
                        rack_subset_keys->keys[empty_count], rack_subset_keys->counts[empty_count], visit
                    );
                };
                count += count_window(checks, neighbors, window, empty_count, visit_groups);
            }
        }
    }
    return count;
}

enum class MaskPolicy { lazy, eager, stop_gap, stop_window };

template <GapTraversal traversal, MaskPolicy policy = MaskPolicy::lazy>
struct WindowCounter {
    const WordTable &dictionary;
    const WordPatternTable &patterns;

    WindowCounter(const WordTable &dictionary, const WordPatternTable &patterns)
        : dictionary(dictionary), patterns(patterns) {}

#ifdef SCRABBLE_WINDOW_VARIANTS
    WindowCounter(const WordTable &dictionary, const WordPatternTable &patterns, const benchmark::Rack &)
        : WindowCounter(dictionary, patterns) {}
#endif

    template <typename Checks, typename VisitGroups>
    size_t operator()(Checks &checks, uint16_t neighbors, const MatchedWindow &window,
                      uint8_t, VisitGroups visit_groups) const
    {
        size_t count = 0;
        constexpr bool stop_at_impossible = policy == MaskPolicy::stop_gap || policy == MaskPolicy::stop_window;
        std::optional<WindowMasks<stop_at_impossible>> masks;
        auto build_masks = [&] {
            if constexpr (traversal == GapTraversal::scan) {
                masks.emplace(checks, window);
            } else if constexpr (traversal == GapTraversal::empty_bits) {
                masks.emplace(checks, window, UINT16_MAX);
            } else {
                masks.emplace(checks, window, neighbors);
            }
            trace_mask_built(masks->ruled_out, masks->gap_count);
        };
        if constexpr (policy == MaskPolicy::eager) {
            build_masks();
        }
        visit_groups([&](const WordPatternAnagramGroup &group) {
            if (policy != MaskPolicy::eager && !masks) {
                build_masks();
            }
            const GroupWords words{dictionary, patterns.word_indices.data() + group.word_indices.start,
                                   group.word_indices.count, checks.row, window.start};
            if (masks->ruled_out) {
                words.decide(false);
                return policy == MaskPolicy::stop_window ? GroupSearch::stop_window : GroupSearch::keep_going;
            }
            count += words.check_words(*masks);
            return GroupSearch::keep_going;
        });
        return count;
    }
};

using PackedCounter = WindowCounter<GapTraversal::scan>;

#ifdef SCRABBLE_WINDOW_VARIANTS
struct OneGapCounter : PackedCounter {
    using PackedCounter::PackedCounter;

    template <typename VisitGroups>
    size_t operator()(RowCheckSets &checks, uint16_t neighbors, const MatchedWindow &window,
                      uint8_t empty_count, VisitGroups visit_groups) const
    {
        if (empty_count != 1) {
            return PackedCounter::operator()(checks, neighbors, window, empty_count, visit_groups);
        }
        std::optional<uint32_t> letters;
        size_t count = 0;
        visit_groups([&](const WordPatternAnagramGroup &group) {
            if (!letters) {
                const auto gap = window.empty_columns(checks.occupied());
                letters = checks.get(__builtin_ctz(gap));
            }
            const auto tile = get_byte_128(group.rack_letters, 0, BOARD_SIZE);
            const bool accepted = *letters & (1u << (tile - 'A'));
            if (accepted) {
                count += group.word_indices.count;
            }
            trace_placements(checks.row, window.start, patterns.word_indices.data() + group.word_indices.start,
                             group.word_indices.count, accepted);
            return GroupSearch::keep_going;
        });
        return count;
    }
};

struct RackWindowChecks : GapChecks {
    bool ruled_out = false;

    RackWindowChecks(RowCheckSets &checks, const MatchedWindow &window, uint32_t rack_letters)
    {
        for (uint8_t offset = 0; offset < window.length; ++offset) {
            const uint8_t col = window.start + offset;
            if (checks.occupied() & bit_at_u16(col)) {
                continue;
            }
            const uint32_t allowed = checks.get(col);
            const uint32_t eligible = allowed & rack_letters;
            if (!eligible) {
                ruled_out = true;
                break;
            }
            if (eligible == rack_letters) {
                trace_rack_gap_removed(allowed != PerpendicularCheckTable::all_letters);
            } else {
                gaps[gap_count++] = {offset, eligible};
            }
        }
        trace_mask_built(ruled_out, gap_count);
    }
};

enum class SubsetPolicy { none, alphabet, capacity, capacity_multi };

static bool has_tile_capacity(const GapChecks &checks, PatternBytes key, uint8_t tile_count)
{
    for (uint8_t i = 1; i < checks.gap_count; ++i) {
        const uint32_t letters = checks.gaps[i].letters;
        unsigned needed = 1;
        for (uint8_t j = 0; j < i; ++j) {
            needed += checks.gaps[j].letters == letters;
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
static size_t count_subset_words(const GroupWords &words, const GapChecks &window, PatternBytes key, uint8_t tile_count)
{
    uint32_t alphabet = 0;
    for (uint8_t i = 0; i < tile_count; ++i) {
        alphabet |= 1u << (get_byte_128(key, i, BOARD_SIZE) - 'A');
    }

    GapChecks subset;
    unsigned removed = 0;
    for (uint8_t i = 0; i < window.gap_count; ++i) {
        const auto &gap = window.gaps[i];
        const uint32_t eligible = gap.letters & alphabet;
        if (!eligible) {
            trace_subset(removed, benchmark::SubsetRejection::disjoint);
            return words.decide(false);
        }
        if (eligible == alphabet) {
            ++removed;
        } else {
            subset.gaps[subset.gap_count++] = {gap.offset, eligible};
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

template <SubsetPolicy policy, bool bulk_unrestricted = true>
struct RackCounter {
    const WordTable &dictionary;
    const WordPatternTable &patterns;
    uint32_t rack_letters;

    RackCounter(const WordTable &dictionary, const WordPatternTable &patterns, uint32_t letters)
        : dictionary(dictionary), patterns(patterns), rack_letters(letters) {}

    RackCounter(const WordTable &dictionary, const WordPatternTable &patterns, const benchmark::Rack &rack)
        : RackCounter(dictionary, patterns, 0)
    {
        for (uint8_t i = 0; i < rack.length; ++i) {
            rack_letters |= 1u << (get_byte_64(rack.tiles, i) - 'A');
        }
    }

    template <typename VisitGroups>
    size_t operator()(RowCheckSets &checks, uint16_t, const MatchedWindow &window,
                      uint8_t empty_count, VisitGroups visit_groups) const
    {
        size_t count = 0;
        std::optional<RackWindowChecks> masks;
        visit_groups([&](const WordPatternAnagramGroup &group) {
            if (!masks) {
                masks.emplace(checks, window, rack_letters);
            }
            const GroupWords words{dictionary, patterns.word_indices.data() + group.word_indices.start,
                                   group.word_indices.count, checks.row, window.start};
            if (masks->ruled_out) {
                words.decide(false);
                return GroupSearch::stop_window;
            }
            if (bulk_unrestricted && !masks->gap_count) {
                count += words.decide(true);
            } else if constexpr (policy == SubsetPolicy::none) {
                count += words.check_words(*masks);
            } else if (policy == SubsetPolicy::capacity_multi && words.size == 1) {
                count += words.check_words(*masks);
            } else {
                count += count_subset_words<policy>(words, *masks, group.rack_letters, empty_count);
            }
            return GroupSearch::keep_going;
        });
        return count;
    }
};

template <bool bulk_unrestricted>
struct AllLettersCounter : RackCounter<SubsetPolicy::none, bulk_unrestricted> {
    AllLettersCounter(const WordTable &dictionary, const WordPatternTable &patterns, const benchmark::Rack &)
        : RackCounter<SubsetPolicy::none, bulk_unrestricted>(dictionary, patterns, PerpendicularCheckTable::all_letters) {}
};

template <typename Counter, typename Checks = RowCheckSets>
benchmark::WindowSolve measure_solve(const WordTable &words, const WordPatternTable &patterns,
                                     Board &board, const benchmark::Rack &rack)
{
#ifndef SCRABBLE_TRACE_PLACEMENTS
    using Clock = std::chrono::steady_clock;
    auto start = Clock::now();
#endif
    auto keys = rack_subset_keys_for_rack(rack.tiles, rack.length);
    Counter counter(words, patterns, rack);
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
#endif
}

#ifdef SCRABBLE_WINDOW_VARIANTS
const benchmark::WindowVariant &benchmark::window_variant(std::string_view name)
{
    static constexpr WindowVariant variants[] = {
        {"packed", measure_solve<PackedCounter>},
        {"eager", measure_solve<WindowCounter<GapTraversal::scan, MaskPolicy::eager>>},
        {"stop-gap", measure_solve<WindowCounter<GapTraversal::scan, MaskPolicy::stop_gap>>},
        {"stop-window", measure_solve<WindowCounter<GapTraversal::scan, MaskPolicy::stop_window>>},
        {"constrained-only", measure_solve<AllLettersCounter<false>>},
        {"all-letters", measure_solve<AllLettersCounter<true>>},
        {"rack", measure_solve<RackCounter<SubsetPolicy::none>>},
        {"subset", measure_solve<RackCounter<SubsetPolicy::alphabet>>},
        {"subset-capacity", measure_solve<RackCounter<SubsetPolicy::capacity>>},
        {"subset-multi", measure_solve<RackCounter<SubsetPolicy::capacity_multi>>},
        {"row-cache", measure_solve<PackedCounter, CachedRowCheckSets>},
        {"one-gap", measure_solve<OneGapCounter>},
        {"empty-bits", measure_solve<WindowCounter<GapTraversal::empty_bits>>},
        {"neighbor-bits", measure_solve<WindowCounter<GapTraversal::neighbor_bits>>},
    };
    for (const auto &variant : variants) {
        if (variant.name == name) {
            return variant;
        }
    }
    throw std::invalid_argument("unknown window counter: " + std::string(name));
}
#endif

size_t rack_oriented_full_board_solver(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const RackSubsetKeys *rack_subset_keys,
    const Board *board,
    uint8_t rack_length
)
{
    return count_rack_windows(word_patterns, rack_subset_keys, board, rack_length,
                              PackedCounter(*dictionary, *word_patterns));
}

size_t rack_oriented_full_board_solver(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const WordIndexStartRowTable *word_start_rows,
    RackSubsetKeys *rack_subset_keys,
    Board *board,
    uint8_t rack_length
)
{
    const Board original_board = *board;
    return count_rack_windows(word_patterns, rack_subset_keys, board, rack_length,
        [&](const RowCheckSets &checks, uint16_t, const MatchedWindow &window, uint8_t, auto visit_groups) {
            size_t count = 0;
            visit_groups([&](const WordPatternAnagramGroup &group) {
                count += validate_rack_window_words(
                    dictionary, word_patterns, word_start_rows, board,
                    checks.row, window.start, window.length, group, &original_board
                );
                return GroupSearch::keep_going;
            });
            return count;
        });
}

DirectionalCounts rack_oriented_both_directions(
    const WordTable *dictionary,
    const WordPatternTable *word_patterns,
    const RackSubsetKeys *rack_subset_keys,
    Board *board,
    uint8_t rack_length
)
{
    return count_both_views(*board, [&] {
        return rack_oriented_full_board_solver(dictionary, word_patterns, rack_subset_keys, board, rack_length);
    });
}
