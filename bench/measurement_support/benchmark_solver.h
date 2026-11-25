#ifndef BENCHMARK_SOLVER_H
#define BENCHMARK_SOLVER_H

#include "rack_oriented_solver.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace benchmark {
struct PatternCache {
    std::filesystem::path path;
    bool existing;
};

inline PatternCache select_pattern_cache(const WordTable &words, const std::filesystem::path &requested,
                                         const std::filesystem::path &project_root)
{
    const auto path = std::filesystem::absolute(requested);
    if (std::filesystem::exists(path)) {
        return {path, true};
    }
    if (!project_root.empty()) {
        const auto project_cache = project_root / "data/caches/pattern_and_rack_anagram_to_words.cache";
        std::error_code error;
        if (std::filesystem::is_regular_file(project_cache, error) &&
            WordPatternTable::cache_matches_dictionary(words, project_cache.c_str())) {
            return {std::filesystem::absolute(project_cache), true};
        }
    }
    return {path, false};
}

inline std::optional<int> pattern_cache_command(int argc, char **argv)
{
    const bool check = argc == 4 && std::string_view(argv[1]) == "--check-cache";
    const bool select = argc == 5 && std::string_view(argv[1]) == "--select-cache";
    if (!check && !select) {
        return std::nullopt;
    }
    WordTable words = WordTable::from_file(argv[2]);
    if (!words.count) {
        std::fprintf(stderr, "dictionary read failed\n");
        return 1;
    }
    if (check) {
        const bool match = WordPatternTable::cache_matches_dictionary(words, argv[3]);
        std::puts(match ? "match" : "different");
    } else {
        const auto cache = select_pattern_cache(words, argv[4], argv[3]);
        // Everything after the first newline is the native path, including any whitespace.
        std::printf("%s\n%s", cache.existing ? "existing" : "build", cache.path.c_str());
    }
    words.destroy();
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
        if (packed) {
            return packed_both_directions(words, patterns, board, rack.tiles, rack.length);
        }
        return place_and_restore_both_directions(words, patterns, original, board, rack.tiles, rack.length);
    }
};

}
#endif
