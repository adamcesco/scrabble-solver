#include "placement_trace.h"
#include "perpendicular_checks.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static void qualify(const WordTable &words, const char *cache, const fs::path &inputs, const fs::path &output)
{
    auto patterns = std::string_view(cache) == "-" ? WordPatternTable::from_words(words)
                                                  : WordPatternTable::load(words, cache);
    if (patterns.is_empty()) throw std::runtime_error("could not prepare word pattern table");
    PerpendicularCheckTable lookup(words);

    std::cout << "board,across,down\n";
    std::string board_file, letters;
    while (std::cin >> board_file >> letters) {
        Board board = board_from_csv((inputs / board_file).c_str());
        board.prepare_perpendicular_checks(lookup);
        const auto trace = trace_moves(words, patterns, board, SortedRack(letters));
        std::ofstream out;
        out.exceptions(std::ios::badbit | std::ios::failbit);
        out.open(output / board_file);
        out << "direction,row,column,word,accepted\n";
        for (const auto &view : trace.decisions) {
            for (const auto &[down, row, start, word, accepted] : view) {
                out << (down ? "down" : "across") << ',' << (down ? start : row) << ','
                    << (down ? row : start) << ',' << word << ',' << accepted << '\n';
            }
        }
        out.close();
        std::cout << board_file << ',' << trace.counts.across << ',' << trace.counts.down << '\n';
    }
}

int main(int argc, char **argv)
{
    if (argc != 5) {
        std::cerr << "usage: qualification DICTIONARY CACHE|- INPUTS OUTPUT\n";
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
