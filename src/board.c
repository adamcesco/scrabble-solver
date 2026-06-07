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

static void print_bits_compact(uint64_t value, int bit_count)
{
    for (int bit_index = bit_count - 1; bit_index >= 0; --bit_index) {
        putchar((value & ((uint64_t)1 << bit_index)) ? '1' : '0');
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
            if (value > 15u) {
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

static uint32_t get_words_config(const char start_flags[BOARD_SIZE + 1], uint16_t lengths[MAX_NUMBER_OF_WORDS_PER_ROW], uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE])
{
    uint16_t config_mask = 0;

    for (int i = 0; i < BOARD_SIZE; i++) {
        if (isalpha(start_flags[i])) {
            config_mask |= (uint16_t)(1u << i);
        }
    }

    uint16_t index = config_to_index[config_mask];
    uint32_t words_config = (uint32_t)(index & UINT16_C(0x01FF));

    for (int i = 0; i < MAX_NUMBER_OF_WORDS_PER_ROW; ++i) {
        words_config |= (uint32_t)(lengths[i] & UINT16_C(0x000F)) << (9 + (i * 4));
    }

    return words_config;
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

Board board_from_csv(const char *baord_file_path, const char *word_config_file_path, uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE])
{
    Board board = {0};
    FILE *board_file = fopen(baord_file_path, "r");
    char line[BOARD_CSV_LINE_MAX];

    if (board_file == NULL) {
        return board;
    }

    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        char tiles[BOARD_SIZE + 1];

        if (fgets(line, sizeof(line), board_file) == NULL) {
            fclose(board_file);
            return board;
        }

        if (strchr(line, '\n') == NULL && !feof(board_file)) {
            fclose(board_file);
            return board;
        }

        if (!parse_csv_row(line, tiles)) {
            fclose(board_file);
            return board;
        }

        board.rows[row_index] = make_row(tiles);
    }
    
    fclose(board_file);
    
    FILE *word_config_file = fopen(word_config_file_path, "r");

    if (word_config_file == NULL) {
        return board;
    }

    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        char start_flags[BOARD_SIZE + 1];
        uint16_t lengths[MAX_NUMBER_OF_WORDS_PER_ROW];

        if (fgets(line, sizeof(line), word_config_file) == NULL) {
            fclose(word_config_file);
            return board;
        }

        if (strchr(line, '\n') == NULL && !feof(word_config_file)) {
            fclose(word_config_file);
            return board;
        }

        if (!parse_csv_row(line, start_flags)) {
            fclose(word_config_file);
            return board;
        }

        if (!parse_config_csv_row(line, lengths)) {
            fclose(word_config_file);
            return board;
        }

        board.wordsConfigs[row_index] = get_words_config(start_flags, lengths, config_to_index);
    }
    
    fclose(word_config_file);

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

void board_print_words_configs(Board board)
{
    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        uint32_t words_config = board.wordsConfigs[row_index];

        printf("row %d wordsConfig: ", row_index + 1);
        print_bits_compact(words_config & UINT32_C(0x01FF), 9);

        for (int i = 0; i < MAX_NUMBER_OF_WORDS_PER_ROW; ++i) {
            putchar(' ');
            print_bits((words_config >> (9 + (i * 4))) & UINT32_C(0x000F), 4);
        }

        printf(" (index %u", (unsigned int)(words_config & UINT32_C(0x01FF)));

        for (int i = 0; i < MAX_NUMBER_OF_WORDS_PER_ROW; ++i) {
            unsigned int length = (unsigned int)((words_config >> (9 + (i * 4))) & UINT32_C(0x000F));
            printf(", length%d %u", i + 1, length);
        }

        printf(")\n");
    }
}
