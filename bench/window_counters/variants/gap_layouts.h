#ifndef BENCH_WINDOW_COUNTERS_VARIANTS_GAP_LAYOUTS_H
#define BENCH_WINDOW_COUNTERS_VARIANTS_GAP_LAYOUTS_H

#include "solvers/shared/window_masks.h"

namespace rack_solver::experiments {
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
} // namespace rack_solver::experiments

#endif
