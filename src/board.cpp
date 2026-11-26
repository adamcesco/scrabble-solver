#include "board.h"
#include "utils.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <utility>

/* Constants */

#define BOARD_CSV_LINE_MAX 64
#define TILE_BITS ROW_TILE_BITS
#define TILE_MASK ROW_TILE_MASK
#define BLANK_TILE_VALUE 0

static unsigned char tile_at_col(RowTiles packed_tiles, int col_index)
{
    return (unsigned char)((packed_tiles >> row_tile_shift_at_col(col_index)) & TILE_MASK);
}

static RowTiles pack_tiles(const char tiles[BOARD_SIZE + 1])
{
    RowTiles packed = 0;

    for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
        unsigned char tile = (unsigned char)tiles[col_index];

        if (isalpha(tile)) {
            int shift = row_tile_shift_at_col(col_index);
            RowTiles value = (RowTiles)tile << shift;

            packed |= value;
        }
    }

    return packed;
}

static uint16_t make_occupied_mask(const char tiles[BOARD_SIZE + 1])
{
    uint16_t occupied = 0;

    for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
        if (isalpha((unsigned char)tiles[col_index])) {
            occupied |= (uint16_t)(1u << col_index);
        }
    }

    return occupied;
}

Row make_row(const char tiles[BOARD_SIZE + 1])
{
    Row row = {};

    row.tiles = pack_tiles(tiles);
    row.occupiedMask = make_occupied_mask(tiles);

    return row;
}

/* CSV parsing and board loading */

static int parse_csv_row(const char *line, char tiles[BOARD_SIZE + 1])
{
    int col = 0;

    for (int i = 0; line[i] != '\0' && line[i] != '\n'; i++) {
        if (line[i] == ',') {
            continue;
        }

        if (col >= BOARD_SIZE) {
            return 0;
        }

        tiles[col] = line[i];
        col++;
    }

    tiles[col] = '\0';

    return col == BOARD_SIZE;
}

static int read_csv_line(FILE *file, char line[BOARD_CSV_LINE_MAX])
{
    if (fgets(line, BOARD_CSV_LINE_MAX, file) == NULL) {
        return 0;
    }

    return strchr(line, '\n') != NULL || feof(file);
}

static int load_board_rows(Board *board, const char *board_file_path)
{
    FILE *board_file = fopen(board_file_path, "r");
    char line[BOARD_CSV_LINE_MAX];
    char perpendicular_tiles[BOARD_SIZE][BOARD_SIZE + 1];

    if (board_file == NULL) {
        return 0;
    }

    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        char tiles[BOARD_SIZE + 1];

        if (!read_csv_line(board_file, line) || !parse_csv_row(line, tiles)) {
            fclose(board_file);
            return 0;
        }

        board->rows[row_index] = make_row(tiles);

        for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
            perpendicular_tiles[col_index][row_index] = tiles[col_index];
        }
    }

    fclose(board_file);

    for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
        perpendicular_tiles[col_index][BOARD_SIZE] = '\0';
        board->perpendicularRows[col_index] = make_row(perpendicular_tiles[col_index]);
    }

    return 1;
}

Board board_from_csv(const char *board_file_path)
{
    Board board = {};

    if (!load_board_rows(&board, board_file_path)) {
        return Board{};
    }

    return board;
}

void Board::swap_views()
{
    std::swap(rows, perpendicularRows);
    normal_runs.swap(perpendicular_runs);
}

/* Diagnostics */

static void print_tile_chars(RowTiles packed_tiles)
{
    for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
        unsigned char tile = tile_at_col(packed_tiles, col_index);
        char tile_char = tile == BLANK_TILE_VALUE ? '.' : (char)tile;

        printf("   %c    ", tile_char);

        if (col_index < BOARD_SIZE - 1) {
            putchar(' ');
        }
    }
}

void print_row(Row row) {
    print_bits_grouped(stdout, row.tiles, BOARD_SIZE * TILE_BITS, TILE_BITS, 1);
    putchar('\n');
    printf("       ");
    print_tile_chars(row.tiles);
    putchar('\n');
}

static void print_rows(const Row rows[BOARD_SIZE])
{
    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        printf("row %2d ", row_index + 1);
        print_row(rows[row_index]);
    }
}

void board_print(Board board)
{
    print_rows(board.rows);
}

void board_print_perpendicular(Board board)
{
    print_rows(board.perpendicularRows);
}
