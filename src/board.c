#include "board.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Constants */

#define BOARD_CSV_LINE_MAX 64
#define FIRST_TILE_GROUP_SIZE 3
#define LAST_TILE_GROUP_SIZE (BOARD_SIZE - FIRST_TILE_GROUP_SIZE)
#define TILE_BITS 5
#define TILE_MASK UINT64_C(0x1F)
#define BLANK_TILE_VALUE TILE_MASK

/* Row encoding */

static uint64_t tile_mask_at_shift(int shift)
{
    return TILE_MASK << shift;
}

static uint64_t packed_tile_value(unsigned char tile)
{
    return (uint64_t)(tile & TILE_MASK);
}

static uint16_t pack_first3(const char tiles[BOARD_SIZE + 1])
{
    uint16_t packed = UINT16_MAX;

    for (int col_index = 0; col_index < FIRST_TILE_GROUP_SIZE; ++col_index) {
        unsigned char tile = (unsigned char)tiles[col_index];

        if (isalpha(tile)) {
            int shift = (FIRST_TILE_GROUP_SIZE - 1 - col_index) * TILE_BITS;
            uint16_t mask = (uint16_t)tile_mask_at_shift(shift);
            uint16_t value = (uint16_t)(packed_tile_value(tile) << shift);

            packed = (uint16_t)((packed & ~mask) | value);
        }
    }

    return packed;
}

static uint64_t pack_last12(const char tiles[BOARD_SIZE + 1])
{
    uint64_t packed = UINT64_MAX;

    for (int col_index = FIRST_TILE_GROUP_SIZE; col_index < BOARD_SIZE; ++col_index) {
        unsigned char tile = (unsigned char)tiles[col_index];

        if (isalpha(tile)) {
            int shift = (BOARD_SIZE - 1 - col_index) * TILE_BITS;
            uint64_t mask = tile_mask_at_shift(shift);
            uint64_t value = packed_tile_value(tile) << shift;

            packed = (packed & ~mask) | value;
        }
    }

    return packed;
}

static uint64_t make_care_mask(uint64_t packed_tiles, int tile_count)
{
    uint64_t care = 0;

    for (int tile_index = 0; tile_index < tile_count; ++tile_index) {
        int shift = tile_index * TILE_BITS;
        uint64_t mask = tile_mask_at_shift(shift);

        if ((packed_tiles & mask) != mask) {
            care |= mask;
        }
    }

    return care;
}

static uint16_t make_first3_care_mask(uint16_t tiles)
{
    return (uint16_t)make_care_mask(tiles, FIRST_TILE_GROUP_SIZE);
}

static uint64_t make_last12_care_mask(uint64_t tiles)
{
    return make_care_mask(tiles, LAST_TILE_GROUP_SIZE);
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

int row_can_house(Row board_row, Row proposed_row)
{
    return (((board_row.first3Tiles ^ proposed_row.first3Tiles) & board_row.first3CareMask & proposed_row.first3CareMask) == 0)
        && (((board_row.last12Tiles ^ proposed_row.last12Tiles) & board_row.last12CareMask & proposed_row.last12CareMask) == 0);
}

/* Word-start config maps */

static void add_config(uint16_t config, size_t *config_count, uint16_t index_to_config[MAX_NUMBER_OF_START_CONFIGS], uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE])
{
    if (*config_count >= MAX_NUMBER_OF_START_CONFIGS) {
        return;
    }

    index_to_config[*config_count] = config;
    config_to_index[config] = (uint16_t)*config_count;
    (*config_count)++;
}

static void generate_configs_from(int min_start, uint16_t current_config, size_t *config_count, uint16_t index_to_config[MAX_NUMBER_OF_START_CONFIGS], uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE])
{
    add_config(current_config, config_count, index_to_config, config_to_index);

    for (int start = min_start; start <= MAX_START_CONFIG; start++) {
        uint16_t next_config = current_config | (uint16_t)(1u << start);

        generate_configs_from(
            start + NEXT_START_CONFIG_OFFSET,
            next_config,
            config_count,
            index_to_config,
            config_to_index
        );
    }
}

void init_config_maps(uint16_t index_to_config[MAX_NUMBER_OF_START_CONFIGS], uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE])
{
    size_t config_count = 0;

    for (size_t i = 0; i < WORD_START_CONFIG_LOOKUP_SIZE; i++) {
        config_to_index[i] = UINT16_MAX;
    }

    generate_configs_from(0, 0, &config_count, index_to_config, config_to_index);
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

static int parse_config_csv_row(const char *line, uint16_t lengths[MAX_NUMBER_OF_WORDS_PER_ROW])
{
    int length_count = 0;

    for (int i = 0; i < MAX_NUMBER_OF_WORDS_PER_ROW; ++i) {
        lengths[i] = 0;
    }

    for (int i = 0; line[i] != '\0' && line[i] != '\n';) {
        if (line[i] == ',') {
            i++;
            continue;
        }

        if (!isdigit((unsigned char)line[i])) {
            while (line[i] != '\0' && line[i] != '\n' && line[i] != ',') {
                i++;
            }
            continue;
        }

        if (length_count >= MAX_NUMBER_OF_WORDS_PER_ROW) {
            return 0;
        }

        uint16_t value = 0;
        while (isdigit((unsigned char)line[i])) {
            value = (uint16_t)(value * 10u + (uint16_t)(line[i] - '0'));
            if (value > WORD_CONFIG_LENGTH_MASK) {
                return 0;
            }
            i++;
        }

        if (line[i] != '\0' && line[i] != '\n' && line[i] != ',') {
            return 0;
        }

        lengths[length_count] = value;
        length_count++;
    }

    return 1;
}

static uint16_t word_start_mask_from_flags(const char start_flags[BOARD_SIZE + 1])
{
    uint16_t config_mask = 0;

    for (int i = 0; i < BOARD_SIZE; i++) {
        if (isalpha((unsigned char)start_flags[i])) {
            config_mask |= (uint16_t)(1u << i);
        }
    }

    return config_mask;
}

static uint32_t pack_word_config_index(uint16_t index)
{
    return (uint32_t)(index & WORD_CONFIG_INDEX_MASK);
}

static uint32_t pack_word_length(uint16_t length, int length_index)
{
    int shift = WORD_CONFIG_INDEX_BITS + (length_index * WORD_CONFIG_LENGTH_BITS);

    return (uint32_t)(length & WORD_CONFIG_LENGTH_MASK) << shift;
}

static uint32_t get_words_config(const char start_flags[BOARD_SIZE + 1], uint16_t lengths[MAX_NUMBER_OF_WORDS_PER_ROW], uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE])
{
    uint16_t config_mask = word_start_mask_from_flags(start_flags);
    uint16_t index = config_to_index[config_mask];
    uint32_t words_config = pack_word_config_index(index);

    for (int i = 0; i < MAX_NUMBER_OF_WORDS_PER_ROW; ++i) {
        words_config |= pack_word_length(lengths[i], i);
    }

    return words_config;
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

static int load_word_configs(Board *board, const char *word_config_file_path, uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE])
{
    FILE *word_config_file = fopen(word_config_file_path, "r");
    char line[BOARD_CSV_LINE_MAX];

    if (word_config_file == NULL) {
        return 0;
    }

    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        char start_flags[BOARD_SIZE + 1];
        uint16_t lengths[MAX_NUMBER_OF_WORDS_PER_ROW];

        if (!read_csv_line(word_config_file, line)
            || !parse_csv_row(line, start_flags)
            || !parse_config_csv_row(line, lengths)) {
            fclose(word_config_file);
            return 0;
        }

        board->wordsConfigs[row_index] = get_words_config(start_flags, lengths, config_to_index);
    }

    fclose(word_config_file);

    return 1;
}

Board board_from_csv(const char *board_file_path, const char *word_config_file_path, uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE])
{
    Board board = {0};

    if (!load_board_rows(&board, board_file_path)) {
        return (Board){0};
    }

    load_word_configs(&board, word_config_file_path, config_to_index);

    return board;
}

/* Diagnostics */

static void print_bits(uint64_t value, int bit_count)
{
    for (int bit_index = bit_count - 1; bit_index >= 0; --bit_index) {
        putchar((value & ((uint64_t)1 << bit_index)) ? '1' : '0');

        if (bit_index > 0 && bit_index % TILE_BITS == 0) {
            putchar(' ');
        }
    }
}

static void print_bits_compact(uint64_t value, int bit_count)
{
    for (int bit_index = bit_count - 1; bit_index >= 0; --bit_index) {
        putchar((value & ((uint64_t)1 << bit_index)) ? '1' : '0');
    }
}

static void print_tile_chars(uint64_t packed_tiles, int tile_count)
{
    for (int tile_index = 0; tile_index < tile_count; ++tile_index) {
        int shift = (tile_count - 1 - tile_index) * TILE_BITS;
        unsigned char tile = (unsigned char)((packed_tiles >> shift) & TILE_MASK);
        char tile_char = tile == BLANK_TILE_VALUE ? '.' : (char)(tile | 0x40);

        printf("  %c  ", tile_char);

        if (tile_index < tile_count - 1) {
            putchar(' ');
        }
    }
}

void board_print(Board board)
{
    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        Row row = board.rows[row_index];

        printf("row %d first3Tiles: ", row_index + 1);
        print_bits(row.first3Tiles, FIRST_TILE_GROUP_SIZE * TILE_BITS);
        putchar('\n');
        printf("                   ");
        print_tile_chars(row.first3Tiles, FIRST_TILE_GROUP_SIZE);
        putchar('\n');

        printf("row %d last12Tiles: ", row_index + 1);
        print_bits(row.last12Tiles, LAST_TILE_GROUP_SIZE * TILE_BITS);
        putchar('\n');
        printf("                   ");
        print_tile_chars(row.last12Tiles, LAST_TILE_GROUP_SIZE);
        putchar('\n');
    }
}

void board_print_words_configs(Board board)
{
    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        uint32_t words_config = board.wordsConfigs[row_index];

        printf("row %d wordsConfig: ", row_index + 1);
        print_bits_compact(words_config & WORD_CONFIG_INDEX_MASK, WORD_CONFIG_INDEX_BITS);

        for (int i = 0; i < MAX_NUMBER_OF_WORDS_PER_ROW; ++i) {
            int shift = WORD_CONFIG_INDEX_BITS + (i * WORD_CONFIG_LENGTH_BITS);

            putchar(' ');
            print_bits((words_config >> shift) & WORD_CONFIG_LENGTH_MASK, WORD_CONFIG_LENGTH_BITS);
        }

        printf(" (index %u", (unsigned int)(words_config & WORD_CONFIG_INDEX_MASK));

        for (int i = 0; i < MAX_NUMBER_OF_WORDS_PER_ROW; ++i) {
            int shift = WORD_CONFIG_INDEX_BITS + (i * WORD_CONFIG_LENGTH_BITS);
            unsigned int length = (unsigned int)((words_config >> shift) & WORD_CONFIG_LENGTH_MASK);

            printf(", length%d %u", i + 1, length);
        }

        printf(")\n");
    }
}
