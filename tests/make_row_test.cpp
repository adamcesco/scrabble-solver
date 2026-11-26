#include "board.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define TILE_BITS ROW_TILE_BITS
#define TILE_MASK ROW_TILE_MASK
static RowTiles packed_tile(char tile, int shift)
{
    return ((RowTiles)(unsigned char)tile) << shift;
}

static RowTiles tile_mask_at_col(int col_index)
{
    int shift = col_index * TILE_BITS;

    return TILE_MASK << shift;
}

static RowTiles packed_tile_at_col(char tile, int col_index)
{
    int shift = col_index * TILE_BITS;

    return packed_tile(tile, shift);
}

static RowTiles packed_row_with_tile(char tile, int col_index)
{
    return packed_tile_at_col(tile, col_index);
}

static void make_row_packs_blank_tiles_as_zeroes(void)
{
    Row row = make_row("...............");

    assert(row.tiles == 0);
    assert(row.occupiedMask == UINT16_C(0x0000));
}

static void make_row_packs_first_three_tiles_from_left_to_right(void)
{
    Row row = make_row("ABC............");
    RowTiles expected_tiles = packed_tile_at_col('A', 0)
                            | packed_tile_at_col('B', 1)
                            | packed_tile_at_col('C', 2);

    assert(row.tiles == expected_tiles);
    assert(row.occupiedMask == UINT16_C(0x0007));
}

static void make_row_packs_last_twelve_tiles_from_left_to_right(void)
{
    Row row = make_row("...ABCDEFGHIJKL");
    RowTiles expected_tiles = 0;

    for (int col_index = 3; col_index < BOARD_SIZE; ++col_index) {
        expected_tiles |= packed_tile_at_col((char)('A' + col_index - 3), col_index);
    }

    assert(row.tiles == expected_tiles);
    assert(row.occupiedMask == UINT16_C(0x7FF8));
}

static void make_row_keeps_blank_tiles_empty_between_letters(void)
{
    Row row = make_row(".Z.A...........");
    RowTiles expected_tiles = packed_row_with_tile('Z', 1);

    expected_tiles = (expected_tiles & ~tile_mask_at_col(3)) | packed_tile_at_col('A', 3);

    assert(row.tiles == expected_tiles);
    assert(row.occupiedMask == UINT16_C(0x000A));
}

static void make_row_treats_lowercase_letters_as_letters(void)
{
    Row row = make_row("aBc............");
    RowTiles expected_tiles = packed_tile_at_col('a', 0)
                            | packed_tile_at_col('B', 1)
                            | packed_tile_at_col('c', 2);

    assert(row.tiles == expected_tiles);
    assert(row.occupiedMask == UINT16_C(0x0007));
}

static void run_test(const char *name, void (*test)(void))
{
    test();
    printf("PASS %s\n", name);
}

int main(void)
{
    run_test("make_row_packs_blank_tiles_as_zeroes", make_row_packs_blank_tiles_as_zeroes);
    run_test("make_row_packs_first_three_tiles_from_left_to_right", make_row_packs_first_three_tiles_from_left_to_right);
    run_test("make_row_packs_last_twelve_tiles_from_left_to_right", make_row_packs_last_twelve_tiles_from_left_to_right);
    run_test("make_row_keeps_blank_tiles_empty_between_letters", make_row_keeps_blank_tiles_empty_between_letters);
    run_test("make_row_treats_lowercase_letters_as_letters", make_row_treats_lowercase_letters_as_letters);

    return 0;
}
