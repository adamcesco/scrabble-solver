#include "benchmark_solver.h"
#include "solvers/rack_neighbor.h"

#include <chrono>
#include <iostream>

using Clock = std::chrono::steady_clock;

int main(int argc, char **argv)
{
    if (argc != 5 || (std::string_view(argv[1]) != "load" && std::string_view(argv[1]) != "build")) {
        std::cerr << "usage: cache load|build DICTIONARY CACHE BOARD\n";
        return 1;
    }
    auto words = WordTable::from_file(argv[2]);
    if (!words.count) {
        std::cerr << "cannot read dictionary " << argv[2] << '\n';
        return 1;
    }

    WordPatternTable patterns;
    const char *path = argv[3];
    if (std::string_view(argv[1]) == "build") {
        if (std::filesystem::exists(path)) {
            std::cerr << "fresh cache already exists: " << path << '\n';
            return 1;
        }
        const auto start = Clock::now();
        patterns = WordPatternTable::from_words(words);
        const auto built = Clock::now();
        if (patterns.is_empty()) {
            std::cerr << "from_words returned an empty table\n";
            return 1;
        }
        const auto saving = Clock::now();
        if (!patterns.save(words, path)) {
            std::cerr << "cannot save " << path << '\n';
            return 1;
        }
        const auto saved = Clock::now();
        std::cout << std::chrono::duration<double>(built - start).count() << ','
                  << std::chrono::duration<double>(saved - saving).count() << ",,";
    } else {
        const auto start = Clock::now();
        patterns = WordPatternTable::load(words, path);
        const auto loaded = Clock::now();
        if (patterns.is_empty()) {
            std::cerr << "v3 reader rejected " << path << '\n';
            return 1;
        }
        std::cout << ",," << std::chrono::duration<double>(loaded - start).count() << ',';
    }

    const PerpendicularCheckTable checks(words);
    auto board = board_from_csv(argv[4]);
    board.prepare_perpendicular_checks(checks);
    const benchmark::Rack rack("ABFHIKU");
    const auto counts = rack_neighbor_both_directions(words, patterns, board, rack.tiles, rack.length);
    std::cout << counts.across << ',' << counts.down << '\n';
    words.destroy();
}
