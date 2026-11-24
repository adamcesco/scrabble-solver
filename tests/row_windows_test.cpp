#include "row_windows.h"

#include <cassert>
#include <set>
#include <string>
#include <tuple>
#include <utility>

// A multiset preserves duplicate visits while allowing any traversal order.
using Visits = std::multiset<std::tuple<int, std::string, int>>;

static Visits windows_in_row(const Row (&rows)[BOARD_SIZE], uint8_t row_index, uint8_t limit)
{
    Visits visits;
    visit_row_windows(rows, row_index, limit, [&](uint8_t start, const PatternRow &pattern, uint8_t empty_count) {
        std::string tiles(pattern.length, '.');
        for (uint8_t col = 0; col < pattern.length; ++col) {
            char tile = (char)(pattern.tiles >> row_tile_shift_at_col(col));
            if (tile != 0) {
                tiles[col] = tile;
            }
        }
        std::string padded = tiles;
        padded.resize(BOARD_SIZE, '.');
        Row expected = make_row(padded.c_str());
        assert(pattern.occupiedMask == expected.occupiedMask);
        assert(pattern.tiles == expected.tiles);
        visits.emplace(start, tiles, empty_count);
    });
    return visits;
}

static void includes_complete_windows_at_both_edges()
{
    Board board = {};
    board.rows[0] = make_row("AT.............");
    assert((windows_in_row(board.rows, 0, 1) == Visits{{0, "AT.", 1}}));

    board.rows[0] = make_row(".............AT");
    assert((windows_in_row(board.rows, 0, 1) == Visits{{12, ".AT", 1}}));
}

static void visits_empty_spans_connected_above_or_below()
{
    Board board = {};
    board.rows[1] = make_row("AT.............");
    const Visits touching_at{{0, "..", 2}, {1, "..", 2}};
    assert(windows_in_row(board.rows, 0, 2) == touching_at);
    assert(windows_in_row(board.rows, 2, 2) == touching_at);

    board.rows[BOARD_SIZE - 2] = make_row("AT.............");
    assert(windows_in_row(board.rows, BOARD_SIZE - 1, 2) == touching_at);
}

static void requires_a_complete_word_with_at_least_one_new_tile()
{
    Board board = {};
    board.rows[7] = make_row("......CAT......");
    assert((windows_in_row(board.rows, 7, 1) == Visits{{5, ".CAT", 1}, {6, "CAT.", 1}}));

    board.rows[7] = make_row("ABCDEFGHIJKLMNO");
    assert(windows_in_row(board.rows, 7, 7).empty());

    board.rows[7] = make_row("ABCDEFG.IJKLMNO");
    assert((windows_in_row(board.rows, 7, 1) == Visits{{0, "ABCDEFG.IJKLMNO", 1}}));
}

static void skips_disconnected_windows_and_single_squares()
{
    Board board = {};
    for (uint8_t row = 0; row < BOARD_SIZE; ++row) {
        assert(windows_in_row(board.rows, row, 7).empty());
    }
    board.rows[0] = make_row("A..............");
    assert(windows_in_row(board.rows, 1, 1).empty());
}

static void filters_by_empty_count_independently_of_word_length()
{
    Board board = {};
    board.rows[4] = make_row("C..T...........");
    const Visits one_tile{{0, "C.", 1}, {2, ".T", 1}, {3, "T.", 1}};
    Visits two_tiles = one_tile;
    two_tiles.insert({{0, "C..T", 2}, {2, ".T.", 2}, {3, "T..", 2}});
    assert(windows_in_row(board.rows, 4, 2) == two_tiles);
    assert(windows_in_row(board.rows, 4, 1) == one_tile);
    assert(windows_in_row(board.rows, 4, 0).empty());

    board.rows[4] = make_row("A..............");
    const Visits full_scan{
        {0, "A.", 1}, {0, "A..", 2}, {0, "A...", 3}, {0, "A....", 4},
        {0, "A.....", 5}, {0, "A......", 6}, {0, "A.......", 7},
    };
    assert(windows_in_row(board.rows, 4, 7) == full_scan);
}

static void visits_repeated_patterns_at_each_start_in_either_view()
{
    Board board = {};
    board.rows[5] = make_row(".AT..........AT");
    board.perpendicularRows[1] = board.perpendicularRows[13] = make_row(".....A.........");
    board.perpendicularRows[2] = board.perpendicularRows[14] = make_row(".....T.........");
    const Visits expected{{0, ".AT", 1}, {1, "AT.", 1}, {12, ".AT", 1}};
    assert(windows_in_row(board.rows, 5, 1) == expected);

    std::swap(board.rows, board.perpendicularRows);
    assert(windows_in_row(board.perpendicularRows, 5, 1) == expected);
}

int main()
{
    includes_complete_windows_at_both_edges();
    visits_empty_spans_connected_above_or_below();
    requires_a_complete_word_with_at_least_one_new_tile();
    skips_disconnected_windows_and_single_squares();
    filters_by_empty_count_independently_of_word_length();
    visits_repeated_patterns_at_each_start_in_either_view();
}
