#ifndef INCLUDE_SOLVERS_SHARED_H
#define INCLUDE_SOLVERS_SHARED_H

#include "board.h"

#include <stddef.h>
#include <stdint.h>

#define MAX_RACK_TILES 7
#define RACK_SUBSET_KEY_CAPACITY 128

typedef struct {
    uint64_t keys[MAX_RACK_TILES + 1][RACK_SUBSET_KEY_CAPACITY];
    uint8_t counts[MAX_RACK_TILES + 1];
} RackSubsetKeys;

RackSubsetKeys rack_subset_keys_for_rack(uint64_t sorted_rack, uint8_t rack_length);

struct DirectionalCounts {
    size_t across;
    size_t down;
};

// Count the active view, then the perpendicular view, and restore the caller's orientation.
template <typename CountDirection>
DirectionalCounts count_both_views(Board &board, CountDirection count)
{
    size_t across = count();
    board.swap_views();
    size_t down = count();
    board.swap_views();
    return {across, down};
}

#endif
