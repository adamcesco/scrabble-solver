#ifndef SRC_SOLVERS_SHARED_WINDOW_MASKS_H
#define SRC_SOLVERS_SHARED_WINDOW_MASKS_H

#include "rack_windows.h"
#include "perpendicular_checks.h"

namespace rack_solver {
enum class GapTraversal { scan, empty_bits, neighbor_bits };

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

enum class GapEvaluation { skip_unrestricted, early_exit, accumulate };

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
                if (!visit_gap(offset, checks.get(col), alphabet)) {
                    break;
                }
            }
        }
    }

    template <typename Checks, typename Alphabet = AllLetters>
    WindowMasks(Checks &checks, const MatchedWindow &window, uint16_t eligible_columns, Alphabet alphabet = {})
    {
        uint16_t columns = window.empty_columns(checks.occupied()) & eligible_columns;
        while (columns) {
            const uint8_t col = __builtin_ctz(columns);
            if (!visit_gap(col - window.start, checks.get(col), alphabet)) {
                break;
            }
            columns &= columns - 1u;
        }
    }

private:
    // Both traversals stop as soon as the chosen policy rules out the window.
    template <typename Alphabet>
    bool visit_gap(uint8_t offset, uint32_t allowed, Alphabet alphabet)
    {
        const uint32_t letters = alphabet.eligible(allowed);
        if (stop_at_impossible && !letters) {
            ruled_out = true;
            return false;
        }
        if constexpr (constrained_only) {
            if (alphabet.unrestricted(letters)) {
#ifdef SCRABBLE_TRACE_PLACEMENTS
                trace_rack_gap_removed(allowed != PerpendicularCheckTable::all_letters);
#endif
                return true;
            }
        }
        this->append(offset, letters);
        ruled_out |= letters == 0;
        return true;
    }
};
} // namespace rack_solver

#endif
