#ifndef SORTED_RACK_H
#define SORTED_RACK_H

#include "solver.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string_view>

struct SortedRack {
    uint64_t tiles = 0;
    uint8_t length;

    explicit SortedRack(std::string_view letters)
    {
        if (letters.empty() || letters.size() > MAX_RACK_TILES ||
            !std::is_sorted(letters.begin(), letters.end()) ||
            letters.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") != letters.npos) {
            throw std::invalid_argument("expected 1-7 sorted A-Z rack tiles");
        }
        length = letters.size();
        std::memcpy(&tiles, letters.data(), length);
    }
};

#endif
