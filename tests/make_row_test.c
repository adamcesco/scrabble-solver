#include "board.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define TILE_BITS ROW_TILE_BITS
#define TILE_MASK ROW_TILE_MASK
#define ROW_WORD_START_MASK(tiles) make_word_start_mask(make_row(tiles).occupiedMask)
#define ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(letter) \
    assert(ROW_WORD_START_MASK(#letter "..............") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK("." #letter ".............") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK(".." #letter "............") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK("..." #letter "...........") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK("...." #letter "..........") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK("....." #letter ".........") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK("......" #letter "........") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK("......." #letter ".......") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK("........" #letter "......") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK("........." #letter ".....") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK(".........." #letter "....") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK("..........." #letter "...") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK("............" #letter "..") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK("............." #letter ".") == UINT16_C(0x0000)); \
    assert(ROW_WORD_START_MASK(".............." #letter) == UINT16_C(0x0000))

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

static RowTiles care_mask_for_cols(uint16_t occupied)
{
    RowTiles care = 0;

    for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
        if ((occupied & (uint16_t)(1u << col_index)) != 0) {
            care |= tile_mask_at_col(col_index);
        }
    }

    return care;
}

static RowTiles packed_row_with_tile(char tile, int col_index)
{
    return packed_tile_at_col(tile, col_index);
}

static void make_row_packs_blank_tiles_as_zeroes_without_care_masks(void)
{
    Row row = make_row("...............");

    assert(row.tiles == 0);
    assert(row.careMask == 0);
    assert(row.occupiedMask == UINT16_C(0x0000));
}

static void make_row_packs_first_three_tiles_from_left_to_right(void)
{
    Row row = make_row("ABC............");
    RowTiles expected_tiles = packed_tile_at_col('A', 0)
                            | packed_tile_at_col('B', 1)
                            | packed_tile_at_col('C', 2);

    assert(row.tiles == expected_tiles);
    assert(row.careMask == care_mask_for_cols(UINT16_C(0x0007)));
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
    assert(row.careMask == care_mask_for_cols(UINT16_C(0x7FF8)));
    assert(row.occupiedMask == UINT16_C(0x7FF8));
}

static void make_row_keeps_blank_tiles_uncared_between_letters(void)
{
    Row row = make_row(".Z.A...........");
    RowTiles expected_tiles = packed_row_with_tile('Z', 1);

    expected_tiles = (expected_tiles & ~tile_mask_at_col(3)) | packed_tile_at_col('A', 3);

    assert(row.tiles == expected_tiles);
    assert(row.careMask == care_mask_for_cols(UINT16_C(0x000A)));
    assert(row.occupiedMask == UINT16_C(0x000A));
}

static void make_row_treats_lowercase_letters_as_letters(void)
{
    Row row = make_row("aBc............");
    RowTiles expected_tiles = packed_tile_at_col('a', 0)
                            | packed_tile_at_col('B', 1)
                            | packed_tile_at_col('c', 2);

    assert(row.tiles == expected_tiles);
    assert(row.careMask == care_mask_for_cols(UINT16_C(0x0007)));
    assert(row.occupiedMask == UINT16_C(0x0007));
}

static void make_word_start_mask_returns_no_starts_for_empty_row(void)
{
    assert(make_word_start_mask(UINT16_C(0x0000)) == UINT16_C(0x0000));
}

static void make_word_start_mask_ignores_each_single_letter_position(void)
{
    for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
        uint16_t occupied = (uint16_t)(1u << col_index);

        assert(make_word_start_mask(occupied) == UINT16_C(0x0000));
    }
}

static void make_word_start_mask_returns_contiguous_group_starts(void)
{
    assert(make_word_start_mask(UINT16_C(0x4373)) == UINT16_C(0x0111));
}

static void make_word_start_mask_handles_boundary_between_tile_groups(void)
{
    assert(make_word_start_mask(UINT16_C(0x001C)) == UINT16_C(0x0004));
}

static void make_row_word_start_mask_returns_no_starts_for_empty_row(void)
{
    assert(ROW_WORD_START_MASK("...............") == UINT16_C(0x0000));
}

static void make_row_word_start_mask_ignores_each_single_letter_position(void)
{
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(A);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(B);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(C);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(D);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(E);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(F);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(G);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(H);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(I);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(J);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(K);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(L);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(M);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(N);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(O);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(P);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(Q);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(R);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(S);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(T);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(U);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(V);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(W);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(X);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(Y);
    ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(Z);
}

static void make_row_word_start_mask_returns_contiguous_group_starts(void)
{
    assert(ROW_WORD_START_MASK("AB..CDE.FG....Z") == UINT16_C(0x0111));
}

static void make_row_word_start_mask_handles_boundary_between_tile_groups(void)
{
    assert(ROW_WORD_START_MASK("..ABC..........") == UINT16_C(0x0004));
}

static void run_test(const char *name, void (*test)(void))
{
    test();
    printf("PASS %s\n", name);
}

int main(void)
{
    run_test("make_row_packs_blank_tiles_as_zeroes_without_care_masks", make_row_packs_blank_tiles_as_zeroes_without_care_masks);
    run_test("make_row_packs_first_three_tiles_from_left_to_right", make_row_packs_first_three_tiles_from_left_to_right);
    run_test("make_row_packs_last_twelve_tiles_from_left_to_right", make_row_packs_last_twelve_tiles_from_left_to_right);
    run_test("make_row_keeps_blank_tiles_uncared_between_letters", make_row_keeps_blank_tiles_uncared_between_letters);
    run_test("make_row_treats_lowercase_letters_as_letters", make_row_treats_lowercase_letters_as_letters);
    run_test("make_word_start_mask_returns_no_starts_for_empty_row", make_word_start_mask_returns_no_starts_for_empty_row);
    run_test("make_word_start_mask_ignores_each_single_letter_position", make_word_start_mask_ignores_each_single_letter_position);
    run_test("make_word_start_mask_returns_contiguous_group_starts", make_word_start_mask_returns_contiguous_group_starts);
    run_test("make_word_start_mask_handles_boundary_between_tile_groups", make_word_start_mask_handles_boundary_between_tile_groups);
    run_test("make_row_word_start_mask_returns_no_starts_for_empty_row", make_row_word_start_mask_returns_no_starts_for_empty_row);
    run_test("make_row_word_start_mask_ignores_each_single_letter_position", make_row_word_start_mask_ignores_each_single_letter_position);
    run_test("make_row_word_start_mask_returns_contiguous_group_starts", make_row_word_start_mask_returns_contiguous_group_starts);
    run_test("make_row_word_start_mask_handles_boundary_between_tile_groups", make_row_word_start_mask_handles_boundary_between_tile_groups);

    return 0;
}
