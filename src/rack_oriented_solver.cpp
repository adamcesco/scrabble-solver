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
#ifdef __AVX2__
#include <immintrin.h>
#endif
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
static inline void trace_gap_check(uint8_t, bool) {}
static inline void trace_mask_built(bool, unsigned, bool = false) {}
#ifdef SCRABBLE_WINDOW_VARIANTS
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

struct GapDescriptors {
    struct Gap {
        uint8_t offset;
        uint32_t letters;
    };
    Gap entries[MAX_RACK_TILES];

    void set(uint8_t i, uint8_t offset, uint32_t letters) { entries[i] = {offset, letters}; }
    uint8_t offset_at(uint8_t i) const { return entries[i].offset; }
    uint32_t letters_at(uint8_t i) const { return entries[i].letters; }
};

#ifdef SCRABBLE_WINDOW_VARIANTS
struct SingletonGaps : GapDescriptors {
    char required_letter = 0;

    void prepare(bool ruled_out, uint8_t count)
    {
        if (!ruled_out && count == 1) {
            const uint32_t mask = letters_at(0);
            if ((mask & (mask - 1)) == 0) {
                required_letter = 'A' + __builtin_ctz(mask);
            }
        }
    }
};

struct Uint32Gaps {
    static constexpr unsigned offset_shift = 28;
    static_assert(BOARD_SIZE <= 16 && PerpendicularCheckTable::all_letters < (1u << offset_shift));
    uint32_t entries[MAX_RACK_TILES];

    void set(uint8_t i, uint8_t offset, uint32_t letters)
    {
        entries[i] = (uint32_t(offset) << offset_shift) | letters;
    }
    uint8_t offset_at(uint8_t i) const { return entries[i] >> offset_shift; }
    uint32_t letters_at(uint8_t i) const { return entries[i] & PerpendicularCheckTable::all_letters; }
};

struct GapArrays {
    uint8_t offsets[MAX_RACK_TILES];
    uint32_t masks[MAX_RACK_TILES];

    void set(uint8_t i, uint8_t offset, uint32_t letters)
    {
        offsets[i] = offset;
        masks[i] = letters;
    }
    uint8_t offset_at(uint8_t i) const { return offsets[i]; }
    uint32_t letters_at(uint8_t i) const { return masks[i]; }
};
#endif

enum class GapEvaluation { skip_unrestricted, early_exit, accumulate, fixed_one_two, singleton_mask, singleton_byte };
enum class GapOrder { offset, selectivity };

constexpr bool tests_singleton_letter(GapEvaluation evaluation)
{
    return evaluation == GapEvaluation::singleton_mask || evaluation == GapEvaluation::singleton_byte;
}

template <typename Layout = GapDescriptors>
struct GapChecks {
    Layout gaps;
    uint8_t gap_count = 0;

    void append(uint8_t offset, uint32_t letters) { gaps.set(gap_count++, offset, letters); }

    template <GapEvaluation evaluation>
    bool check_gap(const char *word, uint8_t i, bool accepted_so_far) const
    {
        const uint32_t letters = gaps.letters_at(i);
        const bool passes = (evaluation == GapEvaluation::skip_unrestricted &&
                             letters == PerpendicularCheckTable::all_letters) ||
                            (letters & (1u << (word[gaps.offset_at(i)] - 'A')));
        trace_gap_check(i, accepted_so_far && !passes);
        return passes;
    }

    template <GapEvaluation evaluation>
    bool accepts(const char *word, uint8_t count) const
    {
        bool accepted = true;
        for (uint8_t i = 0; i < count; ++i) {
            const bool passes = check_gap<evaluation>(word, i, accepted);
            if constexpr (evaluation == GapEvaluation::accumulate) {
                accepted &= passes;
            } else if (!passes) {
                return false;
            }
        }
        return accepted;
    }
};

#ifdef SCRABBLE_WINDOW_VARIANTS
inline void order_by_selectivity(GapChecks<GapDescriptors> &checks)
{
    const uint8_t count = checks.gap_count;
    auto &entries = checks.gaps.entries;
    if (count < 2) {
        return;
    }
    // Stable insertion sort of at most seven gaps, counting each mask's letters once.
    unsigned char sizes[MAX_RACK_TILES];
    sizes[0] = __builtin_popcount(entries[0].letters);
    for (uint8_t i = 1; i < count; ++i) {
        const auto gap = entries[i];
        const auto size = __builtin_popcount(gap.letters);
        uint8_t j = i;
        while (j > 0 && sizes[j - 1] > size) {
            entries[j] = entries[j - 1];
            sizes[j] = sizes[j - 1];
            --j;
        }
        entries[j] = gap;
        sizes[j] = size;
    }
}

#endif

struct GroupWords {
    const WordTable &dictionary;
    const uint32_t *ids;
    size_t size;
    uint8_t row, start;

    GroupWords(const WordTable &dictionary, const uint32_t *ids, size_t size, uint8_t row, uint8_t start)
        : dictionary(dictionary), ids(ids), size(size), row(row), start(start) {}

    size_t decide(bool accepted) const
    {
        trace_placements(row, start, ids, size, accepted);
        return accepted ? size : 0;
    }

    template <typename Accept>
    bool check_word(size_t i, const char *word, Accept accepts) const
    {
        trace_individual_check();
        const bool accepted = accepts(word);
        trace_placements(row, start, ids + i, 1, accepted);
        return accepted;
    }

    template <typename Accept>
    size_t count_if(Accept accepts, size_t first = 0) const
    {
        size_t count = 0;
        for (size_t i = first; i < size; ++i) {
            count += check_word(i, dictionary.words[ids[i]], accepts);
        }
        return count;
    }

    template <GapEvaluation evaluation, int fixed_count = -1, typename Layout>
    size_t check_gaps(const GapChecks<Layout> &checks) const
    {
        return count_if([&](const char *word) {
            return checks.template accepts<evaluation>(word, fixed_count < 0 ? checks.gap_count : fixed_count);
        });
    }

    template <GapEvaluation evaluation = GapEvaluation::skip_unrestricted, typename Layout>
    size_t check_words(const GapChecks<Layout> &checks) const
    {
        if constexpr (evaluation == GapEvaluation::fixed_one_two) {
            switch (checks.gap_count) {
                case 1: return check_gaps<GapEvaluation::early_exit, 1>(checks);
                case 2: return check_gaps<GapEvaluation::early_exit, 2>(checks);
            }
        }
        if constexpr (tests_singleton_letter(evaluation)) {
            const char required_letter = checks.gaps.required_letter;
            if (required_letter) {
                if constexpr (evaluation == GapEvaluation::singleton_mask) {
                    return check_gaps<GapEvaluation::early_exit, 1>(checks);
                } else {
                    const uint8_t offset = checks.gaps.offset_at(0);
                    return count_if([&](const char *word) {
                        const bool passes = word[offset] == required_letter;
                        trace_gap_check(0, !passes);
                        return passes;
                    });
                }
            }
        }
        return check_gaps<evaluation>(checks);
    }
};

#ifdef SCRABBLE_WINDOW_VARIANTS
#ifdef __AVX2__
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
#endif

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
#endif

struct AllLetters {
    uint32_t eligible(uint32_t allowed) const { return allowed; }
    bool unrestricted(uint32_t eligible) const { return eligible == PerpendicularCheckTable::all_letters; }
};

template <bool stop_at_impossible, bool constrained_only, typename Layout = GapDescriptors>
struct WindowMasks : GapChecks<Layout> {
    bool ruled_out = false;

    template <typename Checks, typename Alphabet = AllLetters>
    WindowMasks(const MatchedWindow &window, Checks &checks, Alphabet alphabet = {})
    {
        for (uint8_t offset = 0; offset < window.length; ++offset) {
            uint8_t col = window.start + offset;
            if (!(checks.occupied() & bit_at_u16(col))) {
                const uint32_t allowed = checks.get(col);
                const uint32_t letters = alphabet.eligible(allowed);
                if (stop_at_impossible && !letters) {
                    ruled_out = true;
                    break;
                }
                if constexpr (constrained_only) {
                    if (alphabet.unrestricted(letters)) {
#ifdef SCRABBLE_TRACE_PLACEMENTS
                        trace_rack_gap_removed(allowed != PerpendicularCheckTable::all_letters);
#endif
                        continue;
                    }
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
        this->append(offset, letters);
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
enum class WindowCounting { all_gaps_per_word, constrained_per_word, bulk_unrestricted };

template <GapTraversal traversal, MaskPolicy policy = MaskPolicy::lazy,
          WindowCounting counting = WindowCounting::all_gaps_per_word, typename Layout = GapDescriptors,
          GapEvaluation evaluation = GapEvaluation::skip_unrestricted, GapOrder order = GapOrder::offset,
          typename Words = GroupWords>
struct WindowCounter {
    const WordTable &dictionary;
    const WordPatternTable &patterns;

    WindowCounter(const WordTable &dictionary, const WordPatternTable &patterns)
        : dictionary(dictionary), patterns(patterns) {}

#ifdef SCRABBLE_WINDOW_VARIANTS
    template <typename OtherLayout>
    using WithGapLayout = WindowCounter<traversal, policy, counting, OtherLayout, evaluation, order, Words>;

    template <GapEvaluation other, GapOrder other_order = order>
    using WithGapEvaluation = WindowCounter<traversal, policy, counting, Layout, other, other_order, Words>;

    template <typename OtherWords>
    using WithWordLoop = WindowCounter<traversal, policy, counting, Layout, evaluation, order, OtherWords>;

    WindowCounter(const WordTable &dictionary, const WordPatternTable &patterns, const benchmark::Rack &)
        : WindowCounter(dictionary, patterns) {}
#endif

    template <typename Checks, typename VisitGroups>
    size_t operator()(Checks &checks, uint16_t neighbors, const MatchedWindow &window,
                      uint8_t, VisitGroups visit_groups) const
    {
        size_t count = 0;
        constexpr bool stop_at_impossible = policy == MaskPolicy::stop_gap || policy == MaskPolicy::stop_window;
        constexpr bool constrained_only = counting != WindowCounting::all_gaps_per_word;
        std::optional<WindowMasks<stop_at_impossible, constrained_only, Layout>> masks;
        auto build_masks = [&] {
            if constexpr (traversal == GapTraversal::scan) {
                masks.emplace(window, checks);
            } else if constexpr (traversal == GapTraversal::empty_bits) {
                masks.emplace(checks, window, UINT16_MAX);
            } else {
                masks.emplace(checks, window, neighbors);
            }
            if constexpr (order == GapOrder::selectivity) {
                order_by_selectivity(*masks);
            }
            bool singleton = false;
            if constexpr (tests_singleton_letter(evaluation)) {
                // Charge both singleton trials for identifying the same required letter once per window.
                masks->gaps.prepare(masks->ruled_out, masks->gap_count);
                singleton = masks->gaps.required_letter != 0;
            }
            trace_mask_built(masks->ruled_out, masks->gap_count, singleton);
        };
        if constexpr (policy == MaskPolicy::eager) {
            build_masks();
        }
        visit_groups([&](const WordPatternAnagramGroup &group) {
            if (policy != MaskPolicy::eager && !masks) {
                build_masks();
            }
            const Words words{dictionary, patterns.word_indices.data() + group.word_indices.start,
                              group.word_indices.count, checks.row, window.start};
            if (masks->ruled_out) {
                words.decide(false);
                return policy == MaskPolicy::stop_window ? GroupSearch::stop_window : GroupSearch::keep_going;
            }
            const bool bulk = counting == WindowCounting::bulk_unrestricted && !masks->gap_count;
            count += bulk ? words.decide(true) : words.template check_words<evaluation>(*masks);
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

struct RackAlphabet {
    uint32_t letters = 0;
    uint32_t eligible(uint32_t allowed) const { return allowed & letters; }
    bool unrestricted(uint32_t eligible) const { return eligible == letters; }
};

enum class SubsetPolicy { none, alphabet, capacity, capacity_multi };

static bool has_tile_capacity(const GapChecks<> &checks, PatternBytes key, uint8_t tile_count)
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
static size_t count_subset_words(const GroupWords &words, const GapChecks<> &window, PatternBytes key, uint8_t tile_count)
{
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
struct RackCounter {
    const WordTable &dictionary;
    const WordPatternTable &patterns;
    RackAlphabet alphabet;

    RackCounter(const WordTable &dictionary, const WordPatternTable &patterns, const benchmark::Rack &rack)
        : dictionary(dictionary), patterns(patterns)
    {
        for (uint8_t i = 0; i < rack.length; ++i) {
            alphabet.letters |= 1u << (get_byte_64(rack.tiles, i) - 'A');
        }
    }

    template <typename VisitGroups>
    size_t operator()(RowCheckSets &checks, uint16_t, const MatchedWindow &window,
                      uint8_t empty_count, VisitGroups visit_groups) const
    {
        size_t count = 0;
        std::optional<WindowMasks</*stop_at_impossible=*/true, /*constrained_only=*/true>> masks;
        visit_groups([&](const WordPatternAnagramGroup &group) {
            if (!masks) {
                masks.emplace(window, checks, alphabet);
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

template <WindowCounting counting>
using StoppingCounter = WindowCounter<GapTraversal::scan, MaskPolicy::stop_window, counting>;

using ScalarCounter = StoppingCounter<WindowCounting::bulk_unrestricted>;
using SingletonCounter = ScalarCounter::WithGapLayout<SingletonGaps>;

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
#ifndef SCRABBLE_TUNING_PAIR
        {"batch-2", measure_solve<PackedCounter::WithWordLoop<BatchedWords<2>>>},
        {"batch-4", measure_solve<PackedCounter::WithWordLoop<BatchedWords<4>>>},
#ifdef __clang__
        {"batch-2-interleave", measure_solve<PackedCounter::WithWordLoop<BatchedWords<2, BatchHint::interleave>>>},
        {"batch-4-interleave", measure_solve<PackedCounter::WithWordLoop<BatchedWords<4, BatchHint::interleave>>>},
#endif
        {"batch-2-prefetch", measure_solve<PackedCounter::WithWordLoop<BatchedWords<2, BatchHint::prefetch>>>},
        {"batch-4-prefetch", measure_solve<PackedCounter::WithWordLoop<BatchedWords<4, BatchHint::prefetch>>>},
        {"early-exit", measure_solve<PackedCounter::WithGapEvaluation<GapEvaluation::early_exit>>},
        {"all-checks", measure_solve<PackedCounter::WithGapEvaluation<GapEvaluation::accumulate>>},
#ifdef __AVX2__
        {"bounded-simd", measure_solve<PackedCounter::WithGapEvaluation<GapEvaluation::accumulate>::WithWordLoop<SimdWords>>},
#endif
        {"selective-first", measure_solve<PackedCounter::WithGapEvaluation<GapEvaluation::early_exit, GapOrder::selectivity>>},
        {"gap-u32", measure_solve<PackedCounter::WithGapLayout<Uint32Gaps>>},
        {"gap-arrays", measure_solve<PackedCounter::WithGapLayout<GapArrays>>},
        {"eager", measure_solve<WindowCounter<GapTraversal::scan, MaskPolicy::eager>>},
        {"stop-gap", measure_solve<WindowCounter<GapTraversal::scan, MaskPolicy::stop_gap>>},
        {"stop-window", measure_solve<StoppingCounter<WindowCounting::all_gaps_per_word>>},
        {"constrained-only", measure_solve<StoppingCounter<WindowCounting::constrained_per_word>>},
        {"all-letters", measure_solve<StoppingCounter<WindowCounting::bulk_unrestricted>>},
        {"scalar-general", measure_solve<ScalarCounter::WithGapEvaluation<GapEvaluation::early_exit>>},
        {"scalar-1-2", measure_solve<ScalarCounter::WithGapEvaluation<GapEvaluation::fixed_one_two>>},
        {"singleton-mask", measure_solve<SingletonCounter::WithGapEvaluation<GapEvaluation::singleton_mask>>},
        {"singleton-byte", measure_solve<SingletonCounter::WithGapEvaluation<GapEvaluation::singleton_byte>>},
#endif
        {"rack", measure_solve<RackCounter<SubsetPolicy::none>>},
#ifndef SCRABBLE_TUNING_PAIR
        {"subset", measure_solve<RackCounter<SubsetPolicy::alphabet>>},
        {"subset-capacity", measure_solve<RackCounter<SubsetPolicy::capacity>>},
        {"subset-multi", measure_solve<RackCounter<SubsetPolicy::capacity_multi>>},
        {"row-cache", measure_solve<PackedCounter, CachedRowCheckSets>},
        {"one-gap", measure_solve<OneGapCounter>},
        {"empty-bits", measure_solve<WindowCounter<GapTraversal::empty_bits>>},
        {"neighbor-bits", measure_solve<WindowCounter<GapTraversal::neighbor_bits>>},
#endif
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
