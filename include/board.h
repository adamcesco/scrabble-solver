#ifndef BOARD_H
#define BOARD_H

#include <limits.h>
#include <memory>
#include <stdint.h>

#define BOARD_SIZE 15
#define MIN_WORD_LEN 2

typedef __uint128_t RowTiles;

#define ROW_TILE_BITS CHAR_BIT
#define ROW_TILE_MASK ((RowTiles)UCHAR_MAX)

static inline int row_tile_shift_at_col(int col_index)
{
    return col_index * ROW_TILE_BITS;
}

typedef struct {
    RowTiles tiles;
    uint16_t occupiedMask;
} Row;

class PerpendicularCheckTable;

struct Board {
    Row rows[BOARD_SIZE];
    Row perpendicularRows[BOARD_SIZE];

    void swap_views();

    // Prepare once for this legal position. The lookup must outlive its prepared boards.
    void prepare_perpendicular_checks(const PerpendicularCheckTable &lookup);
    void prepare_perpendicular_checks(PerpendicularCheckTable &&) = delete;

    // Query an empty square in the prepared board's active view. A is bit 0 and Z is bit 25.
    uint32_t perpendicular_check_set(uint8_t row, uint8_t col) const;

private:
    friend struct SolverBoardSnapshot;
    struct RunStarts;
    std::shared_ptr<const RunStarts> normal_runs, perpendicular_runs;
};

struct PatternRow {
    RowTiles tiles = 0;
    uint16_t occupiedMask = 0;
    uint8_t length = 0;
};

Row make_row(const char tiles[BOARD_SIZE + 1]);

Board board_from_csv(const char *board_file_path);

void print_row(Row row);
void board_print(Board board);
void board_print_perpendicular(Board board);

#endif
