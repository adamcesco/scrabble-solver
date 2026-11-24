#include "placement_comparison.h"
#include "perpendicular_fixtures.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>

using WindowDecisions = std::vector<std::pair<std::string, bool>>;

static WindowDecisions middle_window(const Decisions &decisions)
{
    WindowDecisions result;
    for (const auto &[down, row, start, word, accepted] : decisions) {
        if (row == 1 && start == 0 && word.size() == 3) {
            result.emplace_back(word, accepted);
        }
    }
    return result;
}

static void compare_rt_orders(bool tar_first)
{
    WordTable words = load_dictionary(tar_first ? "AS\nSAT\nTO\nAR\nAT\nTAR\nRAT\n"
                                          : "AS\nSAT\nTO\nAR\nAT\nRAT\nTAR\n");
    {
        PlacementComparison fixture{words};
        for (bool transposed : {false, true}) {
            Board board = board_at(0, 0, {"AS.", ".A.", ".TO"});
            if (transposed) {
                board.swap_views();
            }
            fixture.compare_board("RT", board, "RT");
            board.prepare_perpendicular_checks(fixture.lookup);
            SolverBoardSnapshot snapshot{board};
            RackSubsetKeys rt = rack_for("RT");

            DirectionalCounts counts = rack_oriented_both_directions(
                &words, &fixture.patterns, &rt, &board, 2);
            snapshot.check(board, false);
            for (int pass = 0; pass < 3; ++pass) {
                if (pass > 0) {
                    board.swap_views();
                }
                bool down = pass == 1;
                Decisions decisions = fixture.compare_pass(board, snapshot, down, rt, 2);
                assert((down ? counts.down : counts.across) == accepted_count(decisions));
                if (down == transposed) {
                    const uint32_t r = 1u << ('R' - 'A'), t = 1u << ('T' - 'A');
                    assert((board.perpendicular_check_set(1, 0) & (r | t)) == (r | t));
                    assert((board.perpendicular_check_set(1, 2) & (r | t)) == t);
                    WindowDecisions expected{{"TAR", false}, {"RAT", true}};
                    if (!tar_first) {
                        std::reverse(expected.begin(), expected.end());
                    }
                    assert(middle_window(decisions) == expected);
                }
            }
            snapshot.check(board, false);
            auto [across, down] = fixture.compare_board("unmatched rack", board, "ZZ");
            assert(across.candidates == 0 && down.candidates == 0);
        }
    }
    words.destroy();
}

static void compare_rack_groups_and_ruled_out_windows()
{
    WordTable words = load_dictionary("AS\nASX\nSAT\nTO\nAR\nAT\nTAR\nRAT\n");
    {
        PlacementComparison fixture{words};
        RackSubsetKeys rst = rack_for("RST");
        for (bool blocked : {false, true}) {
            Board board = board_at(0, 0, {blocked ? "ASX" : "AS.", ".A.", ".TO"});
            fixture.compare_board("rack groups", board, "RST");
            board.prepare_perpendicular_checks(fixture.lookup);
            SolverBoardSnapshot snapshot{board};
            Decisions across = fixture.compare_pass(board, snapshot, false, rst, 3);
            // SAT uses ST; TAR and RAT use RT. The X.O crossing disallows all three.
            WindowDecisions actual = middle_window(across);
            std::sort(actual.begin(), actual.end());
            const WindowDecisions expected{{"RAT", !blocked}, {"SAT", !blocked}, {"TAR", false}};
            assert(actual == expected);
            board.swap_views();
            fixture.compare_pass(board, snapshot, true, rst, 3);
            board.swap_views();
            snapshot.check(board, false);
        }
    }
    words.destroy();
}

static void one_tile_counts_in_both_directions_and_restores_the_prepared_board()
{
    WordTable words = load_dictionary("AS\nST\n");
    {
        PlacementComparison fixture{words};
        Board board = board_at(0, 0, {"AS", "S."});
        fixture.compare_board("one tile", board, "T");
        board.prepare_perpendicular_checks(fixture.lookup);
        SolverBoardSnapshot snapshot{board};
        auto rack = rack_for("T");
        const auto counts = rack_oriented_both_directions(&words, &fixture.patterns, &rack, &board, 1);
        assert(counts.across == 1 && counts.down == 1);
        snapshot.check(board, false);
    }
    words.destroy();
}

// The benchmark sends one tab-separated board path and sorted rack per line.
static void qualify_inputs(const char *dictionary_path, const char *cache_path)
{
    WordTable words = WordTable::from_file(dictionary_path);
    assert(words.count > 0);
    {
        PlacementComparison fixture{words};
        assert(!fixture.patterns.is_empty() && fixture.rows.count == words.count);
        if (!fixture.patterns.save(words, cache_path)) {
            throw std::runtime_error("could not save the shared pattern cache");
        }

        std::cout << "across,across_candidates,down,down_candidates\n";
        std::string line;
        while (std::getline(std::cin, line)) {
            size_t tab = line.find('\t');
            assert(tab != std::string::npos);
            std::string path = line.substr(0, tab), letters = line.substr(tab + 1);
            assert(!letters.empty() && letters.size() <= MAX_RACK_TILES);
            Board original = board_from_csv(path.c_str());
            if (std::none_of(std::begin(original.rows), std::end(original.rows),
                             [](const Row &row) { return row.occupiedMask != 0; })) {
                throw std::runtime_error("unreadable or empty post-opening board: " + path);
            }
            auto [across, down] = fixture.compare_board(path.c_str(), original, letters);
            std::cout << across.accepted << ',' << across.candidates << ','
                      << down.accepted << ',' << down.candidates << '\n';
        }
    }
    words.destroy();
}

int main(int argc, char **argv)
{
    compare_rt_orders(true);
    compare_rt_orders(false);
    compare_rack_groups_and_ruled_out_windows();
    one_tile_counts_in_both_directions_and_restores_the_prepared_board();
    if (argc == 3) {
        qualify_inputs(argv[1], argv[2]);
    } else if (argc != 1) {
        return 1;
    }
}
