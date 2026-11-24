#ifndef PLACEMENT_COMPARISON_H
#define PLACEMENT_COMPARISON_H

#include "rack_oriented_solver.h"

#include <array>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

using Decision = std::tuple<bool, int, int, std::string, bool>; // down, row, start, word, accepted
using Decisions = std::vector<Decision>;

struct SolverBoardSnapshot {
    Board original;
    void check(const Board &board, bool down) const;
};

struct PassCounts {
    size_t accepted, candidates;
};

struct PlacementComparison {
    const WordTable &words;
    WordPatternTable patterns = WordPatternTable::from_words(words);
    WordIndexStartRowTable rows = WordIndexStartRowTable::from_words(words);
    PerpendicularCheckTable lookup{words};

    ~PlacementComparison() { rows.destroy(); }

    Decisions compare_pass(Board &board, const SolverBoardSnapshot &snapshot, bool down,
                           RackSubsetKeys &rack, uint8_t length) const;
    std::array<PassCounts, 2> compare_board(const char *name, Board original, const std::string &letters,
                                            const std::optional<Decision> &required_play = {}) const;
};

size_t accepted_count(const Decisions &decisions);
RackSubsetKeys rack_for(std::string letters);

#endif
