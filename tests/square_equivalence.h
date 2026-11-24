#ifndef SQUARE_EQUIVALENCE_H
#define SQUARE_EQUIVALENCE_H

#include "perpendicular_checks.h"
#include "validation_hot.h"

#include <cstdio>
#include <cstdlib>
#include <string>

inline void check_legal_runs(const char *name, const WordTable &dictionary, const Board &board)
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

inline uint32_t validator_set(const WordTable &dictionary, const Board &original, int row, int col)
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

inline std::string letters_in(uint32_t mask)
{
    std::string letters;
    for (char letter = 'A'; letter <= 'Z'; ++letter) {
        if (mask & (1u << (letter - 'A'))) {
            letters.push_back(letter);
        }
    }
    return letters;
}

inline void check_view(const char *name, const char *view, const WordTable &dictionary,
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

#endif
