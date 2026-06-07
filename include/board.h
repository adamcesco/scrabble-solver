#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#define BOARD_SIZE 15
#define MIN_WORD_LEN 2
#define MAX_NUMBER_OF_WORDS_PER_ROW 5

#define WORD_START_CONFIG_LOOKUP_SIZE (1u << BOARD_SIZE)
#define MAX_NUMBER_OF_START_CONFIGS 277
#define MAX_START_CONFIG (BOARD_SIZE - MIN_WORD_LEN)
#define NEXT_START_CONFIG_OFFSET (MIN_WORD_LEN + 1)

#define WORD_CONFIG_INDEX_BITS 9
#define WORD_CONFIG_LENGTH_BITS 4
#define WORD_CONFIG_INDEX_MASK UINT32_C(0x01FF)
#define WORD_CONFIG_LENGTH_MASK UINT32_C(0x000F)
#define WORD_START_POSITION_UNUSED UINT16_MAX

typedef struct {
    uint16_t first3Tiles;
    uint16_t first3CareMask;
    uint16_t occupiedMask; // when or-ed with another Row's occupiedMask, assuming that this row can house the other, it returned the new occupiedMask for that row.
    uint64_t last12Tiles;
    uint64_t last12CareMask;
} Row;

typedef struct {
    Row rows[BOARD_SIZE];
    uint32_t wordsConfigs[BOARD_SIZE];
} Board;

Row make_row(const char tiles[BOARD_SIZE + 1]);
uint16_t make_word_start_mask(uint16_t occupied);
int row_can_house(Row board_row, Row proposed_row);

void init_config_maps(
    uint16_t index_to_config[MAX_NUMBER_OF_START_CONFIGS], // when given an index, it provides an 15 bit map of the word-start configuration associated with that index
    uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE], // when given a word-start configuration, it provides an index of that configuration to be used with index_to_config
    uint16_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW] // when given a word-start configuration, it provides a list of all numeric starting positions for each word (0 - 14)
);

Board board_from_csv(const char *board_file_path, const char *word_config_file_path, uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE]);

void board_print(Board board);
void board_print_words_configs(Board board);

#endif
