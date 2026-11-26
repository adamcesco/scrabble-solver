#ifndef PLACEMENT_COMPARISON_H
#define PLACEMENT_COMPARISON_H

#include "window_variants.h"

#include <array>
#include <functional>
#include <iosfwd>
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

using LookupCounts = std::array<std::array<unsigned, BOARD_SIZE>, BOARD_SIZE>;

struct WindowWorkload;

struct WorkloadOutput {
    std::ostream &stream;
    size_t position = 0;
    void write_header() const;
};

struct PlacementTrace {
    const WordTable &dictionary;
    Board &board;
    const SolverBoardSnapshot &snapshot;
    ViewDecisions decisions{};
    std::array<LookupCounts, 2> lookups{};
    WindowWorkload *workload = nullptr;

    bool down() const { return snapshot.is_down(board); }
    DirectionalCounts solve(const std::function<DirectionalCounts()> &run);
    DirectionalCounts solve(const benchmark::WindowVariant &variant, const WordPatternTable &patterns,
                            const benchmark::Rack &rack);
};

struct PlacementComparison {
    const WordTable &words;
    WordPatternTable patterns = WordPatternTable::from_words(words);
    WordIndexStartRowTable rows = WordIndexStartRowTable::from_words(words);
    PerpendicularCheckTable lookup{words};

    ~PlacementComparison() { rows.destroy(); }

    ViewDecisions compare_board(const char *name, Board original, std::string letters,
                                const WindowVariants &variants = {&benchmark::window_variant("packed")},
                                const std::optional<Decision> &required_play = {},
                                WorkloadOutput *profile = nullptr) const;
};

size_t accepted_count(const Decisions &decisions);

#endif
