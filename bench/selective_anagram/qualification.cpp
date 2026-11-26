#include "placement_comparison.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

static void qualify(const WordTable &words, const char *cache, const fs::path &inputs, const fs::path &output)
{
    PlacementComparison tables{words, WordPatternTable::load(words, cache)};
    if (tables.patterns.is_empty() || tables.rows.count != words.count) {
        throw std::runtime_error("could not load the dictionary and v3 cache");
    }

    std::cout << "board,solver,across,down\n";
    std::string name, board_file, letters;
    while (std::cin >> name >> board_file >> letters) {
        Board board = board_from_csv((inputs / board_file).c_str());
        board.prepare_perpendicular_checks(tables.lookup);
        const SolverBoardSnapshot snapshot{board};
        const benchmark::Rack rack(letters);

        auto record = [&](const char *solver, auto solve) {
            try {
                PlacementTrace trace{words, board, snapshot};
                const auto counts = trace.solve(solve);
                std::ofstream out;
                out.exceptions(std::ios::badbit | std::ios::failbit);
                out.open(output / solver / board_file);
                out << "direction,row,column,word,accepted\n";
                for (const auto &view : trace.decisions) {
                    for (const auto &[down, row, start, word, accepted] : view) {
                        out << (down ? "down" : "across") << ',' << (down ? start : row) << ','
                            << (down ? row : start) << ',' << word << ',' << accepted << '\n';
                    }
                }
                out.close();
                std::cout << board_file << ',' << solver << ',' << counts.across << ',' << counts.down << '\n';
            } catch (const std::exception &error) {
                throw std::runtime_error(name + " " + solver + ": " + error.what());
            }
        };

        record("rack-neighbor", [&] {
            return rack_neighbor_both_directions(words, tables.patterns, board, rack.tiles, rack.length);
        });
        record("packed", [&] {
            return packed_both_directions(words, tables.patterns, board, rack.tiles, rack.length);
        });
        record("place-and-restore", [&] {
            return place_and_restore_both_directions(words, tables.patterns, tables.rows, board, rack.tiles, rack.length);
        });
    }
}

int main(int argc, char **argv)
{
    if (argc != 5) {
        std::cerr << "usage: qualification DICTIONARY CACHE INPUTS OUTPUT\n";
        return 1;
    }
    try {
        auto words = WordTable::from_file(argv[1]);
        qualify(words, argv[2], argv[3], argv[4]);
        words.destroy();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
