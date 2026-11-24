#include "perpendicular_checks.h"
#include "perpendicular_fixtures.h"
#include "square_equivalence.h"

#include <cassert>
#include <string_view>
#include <utility>

// The named crossing runs horizontally on the original board, so a down play tests its gap.
static void check_fixture(const char *name, const WordTable &dictionary, const PerpendicularCheckTable &lookup,
                          Board original, int gap_row, int gap_col, std::string_view expected_letters)
{
    check_legal_runs(name, dictionary, original);
    Board prepared = original;
    prepared.prepare_perpendicular_checks(lookup);
    check_view(name, "across", dictionary, original, prepared);

    prepared.swap_views();
    std::swap(original.rows, original.perpendicularRows);
    check_view(name, "down", dictionary, original, prepared);
    assert(letters_in(prepared.perpendicular_check_set(gap_col, gap_row)) == expected_letters);

    prepared.swap_views();
    std::swap(original.rows, original.perpendicularRows);
    check_view(name, "restored across", dictionary, original, prepared);
}

static void check_crossings(const WordTable &dictionary)
{
    PerpendicularCheckTable lookup(dictionary);
    check_fixture("C.T", dictionary, lookup, board_at(5, 7, {"C.T", "A.E", "TAN"}), 5, 8, "AO");
    check_fixture("C.G", dictionary, lookup, board_at(5, 7, {"C.G", "A.E", "TAN"}), 5, 8, "");
    check_fixture(".T", dictionary, lookup, board_at(6, 0, {".C", "COUNTEREXAMPLE", ".T"}), 8, 0, "A");
    check_fixture("OX.T", dictionary, lookup, board_at(0, 0, {
        "...C", "...O", "...U", "COUNTEREXAMPLE", "OX.T", "T..E", "...R",
        "...E", "...X", "...A", "...M", "...P", "...L", "...E"
    }), 4, 2, "");
    check_fixture("FE.", dictionary, lookup, board_at(7, 7, {"FE"}), 7, 9, "Z");
    check_fixture("C...T", dictionary, lookup, board_at(6, 0, {
        ".........C...T", "COUNTEREXAMPLE", ".........T...N"
    }), 6, 11, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    check_fixture("COUNTEREXAMPLE.", dictionary, lookup, board_at(7, 0, {"COUNTEREXAMPLE"}), 7, 14, "S");
}

int main()
{
    WordTable dictionary = load_dictionary("AT\nCAT\nCOT\nCOUNTEREXAMPLE\nCOUNTEREXAMPLES\n"
                                           "DOG\nFE\nFEZ\nGEN\nOX\nTAN\nTEN\n");
    check_crossings(dictionary);
    dictionary.destroy();

    WordTable atlas = load_dictionary("AT\nAS\nATLAS\nTEN\nART\nNET\n");
    check_fixture("AT.AS", atlas, PerpendicularCheckTable(atlas),
                  board_at(7, 7, {"AT.AS", ".E.R.", ".NET."}), 7, 9, "L");
    atlas.destroy();
}
