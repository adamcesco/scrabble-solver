#include "benchmark_solver.h"
#include "solvers/rack_neighbor.h"

#include <chrono>
#include <iostream>
#include <utility>
#include <vector>

struct Position {
    Board board;
    benchmark::Rack rack;
};

// One process per revision. Relative board paths are from the frozen reference inputs.
int main(int argc, char **argv)
{
    if (argc != 3) {
        std::cerr << "usage: timing-worker DICTIONARY V3_CACHE\n";
        return 1;
    }
    auto words = WordTable::from_file(argv[1]);
    auto patterns = WordPatternTable::load(words, argv[2]);
    if (patterns.is_empty()) {
        std::cerr << "could not load the requested v3 cache\n";
        return 1;
    }
    PerpendicularCheckTable checks(words);
    std::vector<Position> positions;
    size_t count;
    if (!(std::cin >> count) || count == 0) return 1;
    for (size_t i = 0; i < count; ++i) {
        std::string path, letters;
        if (!(std::cin >> path >> letters)) return 1;
        auto board = board_from_csv(path.c_str());
        board.prepare_perpendicular_checks(checks);
        positions.push_back({std::move(board), benchmark::Rack(letters)});
    }
    std::cout << "ready " << positions.size() << std::endl;

    using Clock = std::chrono::steady_clock;
    size_t i;
    while (std::cin >> i) {
        auto &position = positions.at(i);
        const auto start = Clock::now();
        const auto counts = rack_neighbor_both_directions(words, patterns, position.board,
                                                          position.rack.tiles, position.rack.length);
        const auto end = Clock::now();
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        std::cout << i << ' ' << ns << ' ' << counts.across << ' ' << counts.down << std::endl;
    }
    words.destroy();
}
