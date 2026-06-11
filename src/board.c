#include "board.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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

static RowTiles make_care_mask(const char tiles[BOARD_SIZE + 1])
{
    RowTiles care = 0;

    for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
        if (isalpha((unsigned char)tiles[col_index])) {
            care |= row_tile_mask_at_col(col_index);
        }
    }

    return care;
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
    Row row = {
        .tiles = pack_tiles(tiles),
    };

    row.careMask = make_care_mask(tiles);
    row.occupiedMask = make_occupied_mask(tiles);

    return row;
}

/* starting positions for words configurations map */

static void set_config_start_positions(uint16_t config, uint8_t start_positions[MAX_NUMBER_OF_WORDS_PER_ROW + 1])
{
    size_t position_count = 0;

    for (uint16_t start = 0; start < BOARD_SIZE; ++start) {
        if ((config & (uint16_t)(1u << start)) == 0) {
            continue;
        }

        if (position_count >= MAX_NUMBER_OF_WORDS_PER_ROW) {
            return;
        }

        position_count++;
        start_positions[position_count] = (uint8_t)start;
    }

    start_positions[0] = (uint8_t)position_count;
}

static void add_config(
    uint16_t config,
    size_t *config_count,
    uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1]
)
{
    if (*config_count >= MAX_NUMBER_OF_START_CONFIGS) {
        return;
    }

    set_config_start_positions(config, config_to_start_positions[config]);
    (*config_count)++;
}

static void generate_configs_from(
    int min_start,
    uint16_t current_config,
    size_t *config_count,
    uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1]
)
{
    add_config(current_config, config_count, config_to_start_positions);

    for (int start = min_start; start <= MAX_START_CONFIG; start++) {
        uint16_t next_config = current_config | (uint16_t)(1u << start);

        generate_configs_from(
            start + NEXT_START_CONFIG_OFFSET,
            next_config,
            config_count,
            config_to_start_positions
        );
    }
}

void init_config_map(
    uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1]
)
{
    size_t config_count = 0;

    for (size_t i = 0; i < WORD_START_CONFIG_LOOKUP_SIZE; i++) {
        config_to_start_positions[i][0] = 0;
        for (size_t j = 1; j <= MAX_NUMBER_OF_WORDS_PER_ROW; j++) {
            config_to_start_positions[i][j] = WORD_START_POSITION_UNUSED;
        }
    }

    generate_configs_from(0, 0, &config_count, config_to_start_positions);
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
    Board board = {0};

    if (!load_board_rows(&board, board_file_path)) {
        return (Board){0};
    }

    return board;
}

/* Diagnostics */

static void print_bits(RowTiles value, int bit_count)
{
    int tile_count = bit_count / TILE_BITS;

    for (int col_index = 0; col_index < tile_count; ++col_index) {
        int tile_start_bit = col_index * TILE_BITS;

        for (int bit_offset = TILE_BITS - 1; bit_offset >= 0; --bit_offset) {
            int bit_index = tile_start_bit + bit_offset;

            putchar((value & ((RowTiles)1 << bit_index)) ? '1' : '0');
        }

        if (col_index < tile_count - 1) {
            putchar(' ');
        }
    }
}

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
    print_bits(row.tiles, BOARD_SIZE * TILE_BITS);
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
