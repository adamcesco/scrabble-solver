#include "board.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Constants */

#define BOARD_CSV_LINE_MAX 64
#define TILE_BITS 5
#define TILE_MASK ((RowTiles)0x1F)
#define BLANK_TILE_VALUE TILE_MASK

/* Row encoding */

static RowTiles tile_mask_at_shift(int shift)
{
    return TILE_MASK << shift;
}

static RowTiles packed_tile_value(unsigned char tile)
{
    return (RowTiles)(tile & (unsigned char)TILE_MASK);
}

static RowTiles pack_tiles(const char tiles[BOARD_SIZE + 1])
{
    RowTiles packed = (RowTiles)-1;

    for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
        unsigned char tile = (unsigned char)tiles[col_index];

        if (isalpha(tile)) {
            int shift = (BOARD_SIZE - 1 - col_index) * TILE_BITS;
            RowTiles mask = tile_mask_at_shift(shift);
            RowTiles value = packed_tile_value(tile) << shift;

            packed = (packed & ~mask) | value;
        }
    }

    return packed;
}

static RowTiles make_care_mask(RowTiles packed_tiles)
{
    RowTiles care = 0;

    for (int tile_index = 0; tile_index < BOARD_SIZE; ++tile_index) {
        int shift = tile_index * TILE_BITS;
        RowTiles mask = tile_mask_at_shift(shift);

        if ((packed_tiles & mask) != mask) {
            care |= mask;
        }
    }

    return care;
}

static uint16_t make_occupied_mask(Row row)
{
    uint16_t occupied = 0;

    for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
        int shift = (BOARD_SIZE - 1 - col_index) * TILE_BITS;

        occupied |= (uint16_t)(((row.careMask >> shift) & (RowTiles)1) << col_index);
    }

    return occupied;
}

uint16_t make_word_start_mask(uint16_t occupied)
{
    return (uint16_t)(occupied & (uint16_t)~(occupied << 1));
}

Row make_row(const char tiles[BOARD_SIZE + 1])
{
    Row row = {
        .tiles = pack_tiles(tiles),
    };

    row.careMask = make_care_mask(row.tiles);
    row.occupiedMask = make_occupied_mask(row);

    return row;
}

int row_can_house(Row board_row, Row row_with_just_proposed_word)
{
    return (((board_row.tiles ^ row_with_just_proposed_word.tiles)
                & board_row.careMask
                & ((row_with_just_proposed_word.careMask << TILE_BITS) | (row_with_just_proposed_word.careMask >> TILE_BITS))) == 0);
}

Row add_proposed_word_to_row(Row board_row, Row row_with_just_proposed_word)
{
    return (Row){
        .tiles = board_row.tiles & row_with_just_proposed_word.tiles,
        .careMask = board_row.careMask | row_with_just_proposed_word.careMask,
        .occupiedMask = board_row.occupiedMask | row_with_just_proposed_word.occupiedMask,
    };
}

/* Word-start config maps */

static void set_config_start_positions(uint16_t config, uint16_t start_positions[MAX_NUMBER_OF_WORDS_PER_ROW])
{
    size_t position_count = 0;

    for (uint16_t start = 0; start < BOARD_SIZE; ++start) {
        if ((config & (uint16_t)(1u << start)) == 0) {
            continue;
        }

        if (position_count >= MAX_NUMBER_OF_WORDS_PER_ROW) {
            return;
        }

        start_positions[position_count] = start;
        position_count++;
    }
}

static void add_config(
    uint16_t config,
    size_t *config_count,
    uint16_t index_to_config[MAX_NUMBER_OF_START_CONFIGS],
    uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE],
    uint16_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW]
)
{
    if (*config_count >= MAX_NUMBER_OF_START_CONFIGS) {
        return;
    }

    index_to_config[*config_count] = config;
    config_to_index[config] = (uint16_t)*config_count;
    set_config_start_positions(config, config_to_start_positions[config]);
    (*config_count)++;
}

static void generate_configs_from(
    int min_start,
    uint16_t current_config,
    size_t *config_count,
    uint16_t index_to_config[MAX_NUMBER_OF_START_CONFIGS],
    uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE],
    uint16_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW]
)
{
    add_config(current_config, config_count, index_to_config, config_to_index, config_to_start_positions);

    for (int start = min_start; start <= MAX_START_CONFIG; start++) {
        uint16_t next_config = current_config | (uint16_t)(1u << start);

        generate_configs_from(
            start + NEXT_START_CONFIG_OFFSET,
            next_config,
            config_count,
            index_to_config,
            config_to_index,
            config_to_start_positions
        );
    }
}

void init_config_maps(
    uint16_t index_to_config[MAX_NUMBER_OF_START_CONFIGS],
    uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE],
    uint16_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW]
)
{
    size_t config_count = 0;

    for (size_t i = 0; i < WORD_START_CONFIG_LOOKUP_SIZE; i++) {
        config_to_index[i] = UINT16_MAX;
        for (size_t j = 0; j < MAX_NUMBER_OF_WORDS_PER_ROW; j++) {
            config_to_start_positions[i][j] = WORD_START_POSITION_UNUSED;
        }
    }

    generate_configs_from(0, 0, &config_count, index_to_config, config_to_index, config_to_start_positions);
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
    }

    fclose(board_file);

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
    for (int bit_index = bit_count - 1; bit_index >= 0; --bit_index) {
        putchar((value & ((RowTiles)1 << bit_index)) ? '1' : '0');

        if (bit_index > 0 && bit_index % TILE_BITS == 0) {
            putchar(' ');
        }
    }
}

static void print_tile_chars(RowTiles packed_tiles)
{
    for (int tile_index = 0; tile_index < BOARD_SIZE; ++tile_index) {
        int shift = (BOARD_SIZE - 1 - tile_index) * TILE_BITS;
        unsigned char tile = (unsigned char)((packed_tiles >> shift) & TILE_MASK);
        char tile_char = tile == BLANK_TILE_VALUE ? '.' : (char)(tile | 0x40);

        printf("  %c  ", tile_char);

        if (tile_index < BOARD_SIZE - 1) {
            putchar(' ');
        }
    }
}

void board_print(Board board)
{
    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        Row row = board.rows[row_index];

        printf("row %2d ", row_index + 1);
        print_bits(row.tiles, BOARD_SIZE * TILE_BITS);
        putchar('\n');
        printf("       ");
        print_tile_chars(row.tiles);
        putchar('\n');
    }
}
