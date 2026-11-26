#ifndef RACK_SUBSETS_H
#define RACK_SUBSETS_H

#include "solver.h"

struct RackSubsets {
    uint64_t keys[MAX_RACK_TILES + 1][1 << MAX_RACK_TILES];
    uint8_t counts[MAX_RACK_TILES + 1];
};

RackSubsets rack_subsets(uint64_t sorted_rack, uint8_t rack_length);

#endif
