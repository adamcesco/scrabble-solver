#ifndef BENCHMARK_SOLVER_H
#define BENCHMARK_SOLVER_H

#include "rack_oriented_solver.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace benchmark {
inline int check_cache(const char *dictionary, const char *cache)
{
    WordTable words = WordTable::from_file(dictionary);
    if (!words.count) {
        std::fprintf(stderr, "dictionary read failed\n");
        return 1;
    }
    bool match = WordPatternTable::cache_matches_dictionary(words, cache);
    words.destroy();
    std::puts(match ? "match" : "different");
    return 0;
}

struct Rack {
    uint64_t tiles = 0;
    uint8_t length;

    explicit Rack(std::string_view letters)
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

struct Validator {
    const WordTable &words;
    const WordPatternTable &patterns;
    std::optional<PerpendicularCheckTable> packed;
    WordIndexStartRowTable original{};

    Validator(const WordTable &words, const WordPatternTable &patterns, bool use_packed)
        : words(words), patterns(patterns)
    {
        if (use_packed) {
            packed.emplace(words);
        } else {
            original = WordIndexStartRowTable::from_words(words);
            if (original.count != words.count) {
                throw std::runtime_error("original validator construction failed");
            }
        }
    }

    ~Validator() { original.destroy(); }

    void prepare(Board &board) const
    {
        if (packed) {
            board.prepare_perpendicular_checks(*packed);
        }
    }

    DirectionalCounts solve(Board &board, const Rack &rack) const
    {
        auto keys = rack_subset_keys_for_rack(rack.tiles, rack.length);
        if (packed) {
            return rack_oriented_both_directions(&words, &patterns, &keys, &board, rack.length);
        }
        return count_both_views(board, [&] {
            return rack_oriented_full_board_solver(&words, &patterns, &original, &keys, &board, rack.length);
        });
    }
};

}
#endif
