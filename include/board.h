#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#define BOARD_SIZE 15

typedef struct {
    uint16_t first3Tiles;
    uint16_t first3CareMask;
    uint64_t last12Tiles;
    uint64_t last12CareMask;
} Row;

typedef struct {
    Row rows[BOARD_SIZE];
    uint64_t wordStarts[BOARD_SIZE];
} Board;

Board board_from_csv(const char *file_path);
void board_print(Board board);
Row make_row(const char tiles[BOARD_SIZE + 1]);
int row_can_house(Row board_row, Row proposed_row);

#endif
