#ifndef PLACEMENT_TRACE_H
#define PLACEMENT_TRACE_H

#include "sorted_rack.h"

#include <array>
#include <string>
#include <tuple>
#include <vector>

// Coordinates are the row and start column in the direction being searched.
using Decision = std::tuple<bool, int, int, std::string, bool>;
using Decisions = std::vector<Decision>;
using ViewDecisions = std::array<Decisions, 2>;

struct SolverBoardSnapshot {
    Board original;
    bool is_down(const Board &board) const;
    void check(const Board &board) const;
};

struct SolveTrace {
    DirectionalCounts counts;
    ViewDecisions decisions;
};

SolveTrace trace_moves(const WordTable &words, const WordPatternTable &patterns, Board &board, SortedRack rack);
void trace_placements(uint8_t row, uint8_t start, const uint32_t *words, size_t count, bool accepted);

#endif
