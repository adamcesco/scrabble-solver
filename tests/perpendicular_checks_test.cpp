#include "perpendicular_checks.h"
#include "validation_hot.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <unistd.h>

static WordTable load_dictionary(const char *contents)
{
    char path[] = "/tmp/scrabble_perpendicular_checks_XXXXXX";
    FILE *file = fdopen(mkstemp(path), "w");
    assert(file);
    fputs(contents, file);
    fclose(file);
    WordTable dictionary = WordTable::from_file(path);
    unlink(path);
    return dictionary;
}

static Board board_at(int top, int left, std::initializer_list<std::string_view> rows)
{
    char across[BOARD_SIZE][BOARD_SIZE + 1] = {};
    char down[BOARD_SIZE][BOARD_SIZE + 1] = {};
    for (std::string_view row : rows) {
        assert(top < BOARD_SIZE && left + row.size() <= BOARD_SIZE);
        for (size_t offset = 0; offset < row.size(); ++offset) {
            across[top][left + offset] = down[left + offset][top] = row[offset];
        }
        ++top;
    }
    Board board = {};
    for (int i = 0; i < BOARD_SIZE; ++i) {
        board.rows[i] = make_row(across[i]);
        board.perpendicularRows[i] = make_row(down[i]);
    }
    return board;
}

static void check_legal_runs(const char *name, const WordTable &dictionary, const Board &board)
{
    for (const Row *rows : {board.rows, board.perpendicularRows}) {
        for (int row = 0; row < BOARD_SIZE; ++row) {
            std::string run;
            for (int col = 0; col <= BOARD_SIZE; ++col) {
                char tile = col < BOARD_SIZE ? char(rows[row].tiles >> row_tile_shift_at_col(col)) : 0;
                if (tile) {
                    run.push_back(tile);
                } else {
                    if (run.size() >= 2 && !dictionary.contains(run.c_str())) {
                        fprintf(stderr, "%s contains an illegal run %s\n", name, run.c_str());
                        abort();
                    }
                    run.clear();
                }
            }
        }
    }
}

static uint32_t validator_set(const WordTable &dictionary, const Board &original, int row, int col)
{
    uint32_t letters = 0;
    for (char letter = 'A'; letter <= 'Z'; ++letter) {
        Board trial = original;
        Row proposed = {};
        proposed.tiles = RowTiles(letter) << row_tile_shift_at_col(col);
        uint16_t touched;
        if (hot_place_word_onto_perpendicular_rows_and_validate_with_touched_mask(
                &dictionary, original.perpendicularRows, &trial, &proposed, row, col, 1, &touched)) {
            letters |= 1u << (letter - 'A');
        }
    }
    return letters;
}

static std::string letters_in(uint32_t mask)
{
    std::string letters;
    for (char letter = 'A'; letter <= 'Z'; ++letter) {
        if (mask & (1u << (letter - 'A'))) {
            letters.push_back(letter);
        }
    }
    return letters;
}

static void check_view(const char *name, const char *view, const WordTable &dictionary,
                       const Board &original, const Board &prepared)
{
    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            if (original.rows[row].occupiedMask & (1u << col)) {
                continue;
            }
            uint32_t expected = validator_set(dictionary, original, row, col);
            uint32_t actual = prepared.perpendicular_check_set(row, col);
            if (actual != expected) {
                fprintf(stderr, "%s %s (%d,%d) validator only [%s], packed only [%s]\n",
                        name, view, row, col, letters_in(expected & ~actual).c_str(),
                        letters_in(actual & ~expected).c_str());
                abort();
            }
        }
    }
}

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
