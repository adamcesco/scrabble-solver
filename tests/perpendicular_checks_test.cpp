#include "placement_comparison.h"
#include "perpendicular_fixtures.h"
#include "square_equivalence.h"

#include <cassert>
#include <string_view>

// The named crossing runs horizontally on the original board, so a down play tests its gap.
static void check_fixture(const char *name, const PlacementComparison &fixture,
                          Board original, int gap_row, int gap_col, std::string_view expected_letters)
{
    fixture.compare_board(name, original, "ACLOSTZ");
    Board prepared = original;
    prepared.prepare_perpendicular_checks(fixture.lookup);

    prepared.swap_views();
    assert(letters_in(prepared.perpendicular_check_set(gap_col, gap_row)) == expected_letters);

    prepared.swap_views();
    check_view(name, "restored across", fixture.words, original, prepared);
}

static void check_crossings(const WordTable &dictionary)
{
    PlacementComparison fixture{dictionary};
    check_fixture("C.T", fixture, board_at(5, 7, {"C.T", "A.E", "TAN"}), 5, 8, "AO");
    check_fixture("C.G", fixture, board_at(5, 7, {"C.G", "A.E", "TAN"}), 5, 8, "");
    check_fixture(".T", fixture, board_at(6, 0, {".C", "COUNTEREXAMPLE", ".T"}), 8, 0, "A");
    check_fixture("OX.T", fixture, board_at(0, 0, {
        "...C", "...O", "...U", "COUNTEREXAMPLE", "OX.T", "T..E", "...R",
        "...E", "...X", "...A", "...M", "...P", "...L", "...E"
    }), 4, 2, "");
    check_fixture("FE.", fixture, board_at(7, 7, {"FE"}), 7, 9, "Z");
    check_fixture("C...T", fixture, board_at(6, 0, {
        ".........C...T", "COUNTEREXAMPLE", ".........T...N"
    }), 6, 11, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    check_fixture("COUNTEREXAMPLE.", fixture, board_at(7, 0, {"COUNTEREXAMPLE"}), 7, 14, "S");
}

int main()
{
    WordTable dictionary = load_dictionary("AT\nCAT\nCOT\nCOUNTEREXAMPLE\nCOUNTEREXAMPLES\n"
                                           "DOG\nFE\nFEZ\nGEN\nOX\nTAN\nTEN\n");
    check_crossings(dictionary);
    dictionary.destroy();

    WordTable atlas = load_dictionary("AT\nAS\nATLAS\nTEN\nART\nNET\n");
    check_fixture("AT.AS", PlacementComparison{atlas},
                  board_at(7, 7, {"AT.AS", ".E.R.", ".NET."}), 7, 9, "L");
    atlas.destroy();
}
