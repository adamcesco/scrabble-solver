#ifndef BOARD_H
#define BOARD_H

#include <limits.h>
#include <stdint.h>

#define BOARD_SIZE 15
#define MIN_WORD_LEN 2
#define MAX_NUMBER_OF_WORDS_PER_ROW 5

#define WORD_START_CONFIG_LOOKUP_SIZE (1u << BOARD_SIZE)
#define MAX_NUMBER_OF_START_CONFIGS 277
#define MAX_START_CONFIG (BOARD_SIZE - MIN_WORD_LEN)
#define NEXT_START_CONFIG_OFFSET (MIN_WORD_LEN + 1)

#define WORD_START_POSITION_UNUSED UINT8_MAX

typedef __uint128_t RowTiles;

#define ROW_TILE_BITS CHAR_BIT
#define ROW_TILE_MASK ((RowTiles)UCHAR_MAX)

static inline int row_tile_shift_at_col(int col_index)
{
    return col_index * ROW_TILE_BITS;
}

static inline RowTiles row_tile_mask_at_col(int col_index)
{
    return ROW_TILE_MASK << row_tile_shift_at_col(col_index);
}

typedef struct {
    RowTiles tiles;
    RowTiles careMask;
    uint16_t occupiedMask;
} Row;

typedef struct {
    Row rows[BOARD_SIZE];
    Row perpendicularRows[BOARD_SIZE];
} Board;

Row make_row(const char tiles[BOARD_SIZE + 1]);

static inline uint16_t make_word_start_mask(uint16_t occupied)
{
    return (uint16_t)((occupied & (uint16_t)~(occupied << 1)) & (occupied >> 1));
}

// row_with_just_proposed_word should only house the new word that is to be potentially added to board_row
static inline int is_placeable_on_row(const Row *board_row, const Row *row_with_just_proposed_word)
{
    return (((board_row->tiles ^ row_with_just_proposed_word->tiles)
                & board_row->careMask
                & ((row_with_just_proposed_word->careMask << ROW_TILE_BITS) | (row_with_just_proposed_word->careMask >> ROW_TILE_BITS))) == 0)
            && ((board_row->occupiedMask & row_with_just_proposed_word->occupiedMask) != 0);
}

static inline void add_proposed_word_to_row(Row *ouput_row, const Row *board_row, const Row *row_with_just_proposed_word)
{
    ouput_row->tiles = board_row->tiles | row_with_just_proposed_word->tiles;
    ouput_row->careMask = board_row->careMask | row_with_just_proposed_word->careMask;
    ouput_row->occupiedMask = board_row->occupiedMask | row_with_just_proposed_word->occupiedMask;
}

void init_config_map(
    uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1] // when given a word-start configuration, [0] is the number of starts and [1..count] are the numeric starting positions (0 - 14)
);

Board board_from_csv(const char *board_file_path);

void print_row(Row row);
void board_print(Board board);
void board_print_perpendicular(Board board);

#endif
