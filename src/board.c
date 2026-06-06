#include "board.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define BOARD_CSV_LINE_MAX 64

static int parse_csv_row(const char *line, char tiles[BOARD_SIZE])
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

    return col == BOARD_SIZE;
}

static void print_bits(uint64_t value, int bit_count)
{
    for (int bit_index = bit_count - 1; bit_index >= 0; --bit_index) {
        putchar((value & ((uint64_t)1 << bit_index)) ? '1' : '0');

        if (bit_index > 0 && bit_index % 5 == 0) {
            putchar(' ');
        }
    }
}

static void print_tile_chars(uint64_t packed_tiles, int tile_count)
{
    for (int tile_index = 0; tile_index < tile_count; ++tile_index) {
        int shift = (tile_count - 1 - tile_index) * 5;
        unsigned char tile = (unsigned char)((packed_tiles >> shift) & 0x1F);
        char tile_char = tile == 0x1F ? '.' : (char)(tile | 0x40);

        printf("  %c  ", tile_char);

        if (tile_index < tile_count - 1) {
            putchar(' ');
        }
    }
}

Board board_from_csv(const char *file_path)
{
    Board board = {0};
    FILE *file = fopen(file_path, "r");
    char line[BOARD_CSV_LINE_MAX];

    if (file == NULL) {
        return board;
    }

    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        char tiles[BOARD_SIZE];

        if (fgets(line, sizeof(line), file) == NULL) {
            fclose(file);
            return board;
        }

        if (strchr(line, '\n') == NULL && !feof(file)) {
            fclose(file);
            return board;
        }

        if (!parse_csv_row(line, tiles)) {
            fclose(file);
            return board;
        }

        Row row = {
            .first3Tiles = UINT16_MAX,
            .last12Tiles = UINT64_MAX,
        };

        for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
            unsigned char tile = (unsigned char)tiles[col_index];
            uint64_t packed_tile;

            if (!isalpha(tile)) {
                continue;
            }

            packed_tile = (uint64_t)(tile & 0x1F);

            if (col_index < 3) {
                int shift = (2 - col_index) * 5;
                row.first3Tiles = (uint16_t)((row.first3Tiles & ~(uint16_t)(0x1F << shift)) | (uint16_t)(packed_tile << shift));
            } else {
                int shift = (BOARD_SIZE - 1 - col_index) * 5;
                row.last12Tiles = (row.last12Tiles & ~((uint64_t)0x1F << shift)) | (packed_tile << shift);
            }
        }

        board.rows[row_index] = row;
    }

    fclose(file);
    return board;
}

void board_print(Board board)
{
    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        Row row = board.rows[row_index];

        printf("row %d first3Tiles: ", row_index + 1);
        print_bits(row.first3Tiles, 15);
        putchar('\n');
        printf("                   ");
        print_tile_chars(row.first3Tiles, 3);
        putchar('\n');

        printf("row %d last12Tiles: ", row_index + 1);
        print_bits(row.last12Tiles, 60);
        putchar('\n');
        printf("                   ");
        print_tile_chars(row.last12Tiles, BOARD_SIZE - 3);
        putchar('\n');
    }
}
