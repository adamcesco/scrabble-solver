#include "placement_comparison.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::cerr << "usage: reference DICTIONARY CACHE\n";
        return 1;
    }
    auto words = WordTable::from_file(argv[1]);
    {
        PlacementComparison oracle{words, WordPatternTable::load(words, argv[2])};
        if (oracle.patterns.is_empty() || oracle.rows.count != words.count) {
            throw std::runtime_error("could not load dictionary and v3 cache");
        }
        // The runner supplies archived cohort-relative board names and sorted racks.
        std::string name, rack;
        while (std::cin >> name >> rack) {
            const auto board = std::filesystem::path("inputs") / name;
            // The harness returns the place-and-restore trace and checks its counts and
            // board views against the pinned packed solver with assertions enabled.
            const auto decisions = oracle.compare_board(name.c_str(), board_from_csv(board.c_str()), rack);
            std::ofstream out;
            out.exceptions(std::ios::failbit | std::ios::badbit);
            out.open(std::filesystem::path("decisions") / name);
            out << "direction,row,column,word,accepted\n";
            for (const auto &view : decisions) {
                for (const auto &[down, row, start, word, accepted] : view) {
                    out << (down ? "down" : "across") << ',' << (down ? start : row) << ','
                        << (down ? row : start) << ',' << word << ',' << accepted << '\n';
                }
            }
            out.close();
        }
    }
    words.destroy();
}
