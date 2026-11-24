#ifndef PERPENDICULAR_FIXTURES_H
#define PERPENDICULAR_FIXTURES_H

#include "board.h"
#include "word_table.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string_view>
#include <unistd.h>

inline WordTable load_dictionary(const char *contents)
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

inline Board board_at(int top, int left, std::initializer_list<std::string_view> rows)
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

#endif
