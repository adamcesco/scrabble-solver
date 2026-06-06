#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#define BOARD_SIZE 15

typedef struct {
    uint16_t first3Tiles;
    uint64_t last12Tiles;
} Row;

typedef struct {
    Row rows[BOARD_SIZE];
} Board;

Board board_from_csv(const char *file_path);
void board_print(Board board);

#endif
