#include "solver.h"
#include "rack_subsets.h"
#include "row_windows.h"

#include <optional>

#ifdef SCRABBLE_TRACE_PLACEMENTS
#include "placement_trace.h"
#else
static inline void trace_placements(uint8_t, uint8_t, const uint32_t *, size_t, bool) {}
#endif

namespace {
struct MatchedWindow {
    uint8_t start;
    uint8_t length;
    const WordPattern *pattern;
};

struct PerpendicularGaps {
    struct Gap {
        uint8_t offset;
        uint32_t letters;
    };
    Gap gaps[MAX_RACK_TILES];
    uint8_t count = 0;
    bool impossible = false;

    PerpendicularGaps(const Board &board, uint8_t row, const MatchedWindow &window,
                      uint16_t neighbors, uint32_t rack_letters)
    {
        const uint16_t span = low_bit_mask_u16(window.length) << window.start;
        uint16_t columns = span & ~board.rows[row].occupiedMask & neighbors;
        while (columns) {
            const uint8_t col = __builtin_ctz(columns);
            const uint32_t letters = board.perpendicular_check_set(row, col) & rack_letters;
            if (letters == 0) {
                impossible = true;
                break;
            }
            if (letters != rack_letters) {
                gaps[count++] = {static_cast<uint8_t>(col - window.start), letters};
            }
            columns &= columns - 1u;
        }
    }

    bool accepts(const char *word) const
    {
        for (uint8_t i = 0; i < count; ++i) {
            const auto &gap = gaps[i];
            if (!(gap.letters & (1u << (word[gap.offset] - 'A')))) {
                return false;
            }
        }
        return true;
    }
};

size_t count_window(const WordTable &words, const WordPatternTable &patterns, const Board &board,
                    uint8_t row, const MatchedWindow &window, uint16_t neighbors, uint32_t rack_letters,
                    const uint64_t *subsets, uint8_t subset_count)
{
    size_t count = 0;
    std::optional<PerpendicularGaps> gaps;
    auto count_group = [&](const WordPatternAnagramGroup &group) {
        if (!gaps) {
            gaps.emplace(board, row, window, neighbors, rack_letters);
        }
        const uint32_t *ids = patterns.word_indices.data() + group.word_indices.start;
        const size_t size = group.word_indices.count;
        if (gaps->impossible) {
            trace_placements(row, window.start, ids, size, false);
#ifdef SCRABBLE_TRACE_PLACEMENTS
            // Qualification records the rejection of every remaining matching group.
            return true;
#else
            return false;
#endif
        }
        if (gaps->count == 0) {
            count += size;
            trace_placements(row, window.start, ids, size, true);
        } else {
            for (size_t i = 0; i < size; ++i) {
                const bool accepted = gaps->accepts(words.words[ids[i]]);
                count += accepted;
                trace_placements(row, window.start, ids + i, 1, accepted);
            }
        }
        return true;
    };

    const auto range = window.pattern->anagrams;
    if (range.count > 4u * subset_count) {
        for (uint8_t i = 0; i < subset_count; ++i) {
            const auto *group = patterns.get_anagram(*window.pattern, subsets[i]);
            if (group && !count_group(*group)) break;
        }
    } else {
        const auto *group = patterns.anagram_groups.data() + range.start;
        const auto *end = group + range.count;
        uint8_t i = 0;
        while (group != end && i < subset_count) {
            if (group->rack_letters < subsets[i]) {
                ++group;
            } else if (subsets[i] < group->rack_letters) {
                ++i;
            } else {
                if (!count_group(*group)) break;
                ++group;
                ++i;
            }
        }
    }
    return count;
}

size_t count_direction(const WordTable &words, const WordPatternTable &patterns, const Board &board,
                       const RackSubsets &subsets, uint8_t rack_length, uint32_t rack_letters)
{
    size_t count = 0;
    for (uint8_t row = 0; row < BOARD_SIZE; ++row) {
        // For a given start and number of empty squares there is at most one complete window.
        MatchedWindow windows[MAX_RACK_TILES + 1][BOARD_SIZE];
        uint8_t sizes[MAX_RACK_TILES + 1] = {};
        const uint16_t neighbors = visit_row_windows(board.rows, row, rack_length,
            [&](uint8_t start, const PatternRow &pattern, uint8_t empty) {
                if (const auto *match = patterns.get(pattern)) {
                    windows[empty][sizes[empty]++] = {start, pattern.length, match};
                }
            });
        for (uint8_t empty = 1; empty <= rack_length; ++empty) {
            for (uint8_t i = 0; i < sizes[empty]; ++i) {
                count += count_window(words, patterns, board, row, windows[empty][i], neighbors,
                                      rack_letters, subsets.keys[empty], subsets.counts[empty]);
            }
        }
    }
    return count;
}
}

DirectionalCounts count_moves(const WordTable &dictionary, const WordPatternTable &patterns,
                             Board &board, uint64_t sorted_rack, uint8_t rack_length)
{
    const auto subsets = rack_subsets(sorted_rack, rack_length);
    uint32_t letters = 0;
    for (uint8_t i = 0; i < rack_length; ++i) {
        letters |= 1u << (get_byte_64(sorted_rack, i) - 'A');
    }
    const auto across = count_direction(dictionary, patterns, board, subsets, rack_length, letters);
    board.swap_views();
    const auto down = count_direction(dictionary, patterns, board, subsets, rack_length, letters);
    board.swap_views();
    return {across, down};
}
