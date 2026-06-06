#include "board.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define BOARD_CSV_LINE_MAX 64
#define TILE_MASK 0x1F

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
        unsigned char tile = (unsigned char)((packed_tiles >> shift) & TILE_MASK);
        char tile_char = tile == TILE_MASK ? '.' : (char)(tile | 0x40);

        printf("  %c  ", tile_char);

        if (tile_index < tile_count - 1) {
            putchar(' ');
        }
    }
}

static uint16_t pack_first3(const char tiles[BOARD_SIZE + 1])
{
    uint16_t packed = UINT16_MAX;

    for (int col_index = 0; col_index < 3; ++col_index) {
        unsigned char tile = (unsigned char)tiles[col_index];

        if (isalpha(tile)) {
            uint16_t packed_tile = (uint16_t)(tile & TILE_MASK);
            int shift = (2 - col_index) * 5;
            packed = (uint16_t)((packed & ~(uint16_t)(TILE_MASK << shift)) | (uint16_t)(packed_tile << shift));
        }
    }

    return packed;
}

static uint64_t pack_last12(const char tiles[BOARD_SIZE + 1])
{
    uint64_t packed = UINT64_MAX;

    for (int col_index = 3; col_index < BOARD_SIZE; ++col_index) {
        unsigned char tile = (unsigned char)tiles[col_index];

        if (isalpha(tile)) {
            uint64_t packed_tile = (uint64_t)(tile & TILE_MASK);
            int shift = (BOARD_SIZE - 1 - col_index) * 5;
            packed = (packed & ~((uint64_t)TILE_MASK << shift)) | (packed_tile << shift);
        }
    }

    return packed;
}

static uint16_t make_first3_care_mask(uint16_t tiles)
{
    uint16_t care = 0;

    if ((tiles & UINT16_C(0x001F)) != UINT16_C(0x001F)) care |= UINT16_C(0x001F);
    if ((tiles & UINT16_C(0x03E0)) != UINT16_C(0x03E0)) care |= UINT16_C(0x03E0);
    if ((tiles & UINT16_C(0x7C00)) != UINT16_C(0x7C00)) care |= UINT16_C(0x7C00);

    return care;
}

static uint64_t make_last12_care_mask(uint64_t tiles)
{
    uint64_t care = 0;

    if ((tiles & UINT64_C(0x000000000000001F)) != UINT64_C(0x000000000000001F)) care |= UINT64_C(0x000000000000001F);
    if ((tiles & UINT64_C(0x00000000000003E0)) != UINT64_C(0x00000000000003E0)) care |= UINT64_C(0x00000000000003E0);
    if ((tiles & UINT64_C(0x0000000000007C00)) != UINT64_C(0x0000000000007C00)) care |= UINT64_C(0x0000000000007C00);
    if ((tiles & UINT64_C(0x00000000000F8000)) != UINT64_C(0x00000000000F8000)) care |= UINT64_C(0x00000000000F8000);
    if ((tiles & UINT64_C(0x0000000001F00000)) != UINT64_C(0x0000000001F00000)) care |= UINT64_C(0x0000000001F00000);
    if ((tiles & UINT64_C(0x000000003E000000)) != UINT64_C(0x000000003E000000)) care |= UINT64_C(0x000000003E000000);
    if ((tiles & UINT64_C(0x00000007C0000000)) != UINT64_C(0x00000007C0000000)) care |= UINT64_C(0x00000007C0000000);
    if ((tiles & UINT64_C(0x000000F800000000)) != UINT64_C(0x000000F800000000)) care |= UINT64_C(0x000000F800000000);
    if ((tiles & UINT64_C(0x00001F0000000000)) != UINT64_C(0x00001F0000000000)) care |= UINT64_C(0x00001F0000000000);
    if ((tiles & UINT64_C(0x0003E00000000000)) != UINT64_C(0x0003E00000000000)) care |= UINT64_C(0x0003E00000000000);
    if ((tiles & UINT64_C(0x007C000000000000)) != UINT64_C(0x007C000000000000)) care |= UINT64_C(0x007C000000000000);
    if ((tiles & UINT64_C(0x0F80000000000000)) != UINT64_C(0x0F80000000000000)) care |= UINT64_C(0x0F80000000000000);

    return care;
}

Row make_row(const char tiles[BOARD_SIZE + 1])
{
    Row row = {
        .first3Tiles = pack_first3(tiles),
        .last12Tiles = pack_last12(tiles),
    };

    row.first3CareMask = make_first3_care_mask(row.first3Tiles);
    row.last12CareMask = make_last12_care_mask(row.last12Tiles);

    return row;
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
        char tiles[BOARD_SIZE + 1];

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

        board.rows[row_index] = make_row(tiles);
    }

    fclose(file);
    return board;
}

int row_can_house(Row board_row, Row proposed_row)
{
    return  (((board_row.first3Tiles ^ proposed_row.first3Tiles) & board_row.first3CareMask & proposed_row.first3CareMask) == 0)
         && (((board_row.last12Tiles ^ proposed_row.last12Tiles) & board_row.last12CareMask & proposed_row.last12CareMask) == 0);
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
