#ifndef PLACEMENT_COMPARISON_H
#define PLACEMENT_COMPARISON_H

#include "window_variants.h"

#include <array>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

using Decision = std::tuple<bool, int, int, std::string, bool>; // down, row, start, word, accepted
using Decisions = std::vector<Decision>;

struct SolverBoardSnapshot {
    Board original;
    bool is_down(const Board &board) const;
    void check(const Board &board, bool down) const;
};

using ViewDecisions = std::array<Decisions, 2>;
using WindowVariants = std::vector<const benchmark::WindowVariant *>;

struct PlacementComparison {
    const WordTable &words;
    WordPatternTable patterns = WordPatternTable::from_words(words);
    WordIndexStartRowTable rows = WordIndexStartRowTable::from_words(words);
    PerpendicularCheckTable lookup{words};

    ~PlacementComparison() { rows.destroy(); }

    ViewDecisions compare_board(const char *name, Board original, std::string letters,
                                const WindowVariants &variants = {&benchmark::window_variant("packed")},
                                const std::optional<Decision> &required_play = {}) const;
};

size_t accepted_count(const Decisions &decisions);

#endif
