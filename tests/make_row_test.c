#include "board.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define TILE_MASK UINT64_C(0x1F)
#define ROW_WORD_START_MASK(tiles) make_word_start_mask(make_row(tiles).occupiedMask)
#define ASSERT_SINGLE_LETTER_ROW_WORD_START_MASKS(letter) \
    assert(ROW_WORD_START_MASK(#letter "..............") == UINT16_C(0x0001)); \
    assert(ROW_WORD_START_MASK("." #letter ".............") == UINT16_C(0x0002)); \
    assert(ROW_WORD_START_MASK(".." #letter "............") == UINT16_C(0x0004)); \
    assert(ROW_WORD_START_MASK("..." #letter "...........") == UINT16_C(0x0008)); \
    assert(ROW_WORD_START_MASK("...." #letter "..........") == UINT16_C(0x0010)); \
    assert(ROW_WORD_START_MASK("....." #letter ".........") == UINT16_C(0x0020)); \
    assert(ROW_WORD_START_MASK("......" #letter "........") == UINT16_C(0x0040)); \
    assert(ROW_WORD_START_MASK("......." #letter ".......") == UINT16_C(0x0080)); \
    assert(ROW_WORD_START_MASK("........" #letter "......") == UINT16_C(0x0100)); \
    assert(ROW_WORD_START_MASK("........." #letter ".....") == UINT16_C(0x0200)); \
    assert(ROW_WORD_START_MASK(".........." #letter "....") == UINT16_C(0x0400)); \
    assert(ROW_WORD_START_MASK("..........." #letter "...") == UINT16_C(0x0800)); \
    assert(ROW_WORD_START_MASK("............" #letter "..") == UINT16_C(0x1000)); \
    assert(ROW_WORD_START_MASK("............." #letter ".") == UINT16_C(0x2000)); \
    assert(ROW_WORD_START_MASK(".............." #letter) == UINT16_C(0x4000))

static uint64_t packed_tile(char tile, int shift)
{
    return ((uint64_t)(tile & TILE_MASK)) << shift;
}

static void make_row_packs_blank_tiles_as_all_ones_without_care_masks(void)
{
    Row row = make_row("...............");

    assert(row.first3Tiles == UINT16_MAX);
    assert(row.first3CareMask == 0);
    assert(row.occupiedMask == UINT16_C(0x0000));
    assert(row.last12Tiles == UINT64_MAX);
    assert(row.last12CareMask == 0);
}

static void make_row_packs_first_three_tiles_from_left_to_right(void)
{
    Row row = make_row("ABC............");

    assert(row.first3Tiles == UINT16_C(0x8443));
    assert(row.first3CareMask == UINT16_C(0x7FFF));
    assert(row.occupiedMask == UINT16_C(0x0007));
    assert(row.last12Tiles == UINT64_MAX);
    assert(row.last12CareMask == 0);
}

static void make_row_packs_last_twelve_tiles_from_left_to_right(void)
{
    Row row = make_row("...ABCDEFGHIJKL");
    uint64_t expected_tiles = UINT64_C(0xF000000000000000)
                            | packed_tile('A', 55)
                            | packed_tile('B', 50)
                            | packed_tile('C', 45)
                            | packed_tile('D', 40)
                            | packed_tile('E', 35)
                            | packed_tile('F', 30)
                            | packed_tile('G', 25)
                            | packed_tile('H', 20)
                            | packed_tile('I', 15)
                            | packed_tile('J', 10)
                            | packed_tile('K', 5)
                            | packed_tile('L', 0);

    assert(row.first3Tiles == UINT16_MAX);
    assert(row.first3CareMask == 0);
    assert(row.occupiedMask == UINT16_C(0x7FF8));
    assert(row.last12Tiles == expected_tiles);
    assert(row.last12CareMask == UINT64_C(0x0FFFFFFFFFFFFFFF));
}

static void make_row_keeps_blank_tiles_uncared_between_letters(void)
{
    Row row = make_row(".Z.A...........");
    uint64_t expected_last12_tiles = (UINT64_MAX & ~(TILE_MASK << 55)) | packed_tile('A', 55);

    assert(row.first3Tiles == UINT16_C(0xFF5F));
    assert(row.first3CareMask == UINT16_C(0x03E0));
    assert(row.occupiedMask == UINT16_C(0x000A));
    assert(row.last12Tiles == expected_last12_tiles);
    assert(row.last12CareMask == UINT64_C(0x0F80000000000000));
}

static void make_row_treats_lowercase_letters_as_letters(void)
{
    Row row = make_row("aBc............");

    assert(row.first3Tiles == UINT16_C(0x8443));
    assert(row.first3CareMask == UINT16_C(0x7FFF));
    assert(row.occupiedMask == UINT16_C(0x0007));
}

static void make_word_start_mask_returns_no_starts_for_empty_row(void)
{
    assert(make_word_start_mask(UINT16_C(0x0000)) == UINT16_C(0x0000));
}

static void make_word_start_mask_returns_start_for_each_single_letter_position(void)
{
    for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
        uint16_t occupied = (uint16_t)(1u << col_index);

        assert(make_word_start_mask(occupied) == occupied);
    }
}

static void make_word_start_mask_returns_contiguous_group_starts(void)
{
    assert(make_word_start_mask(UINT16_C(0x4373)) == UINT16_C(0x4111));
}

static void make_word_start_mask_handles_boundary_between_tile_groups(void)
{
    assert(make_word_start_mask(UINT16_C(0x001C)) == UINT16_C(0x0004));
}

static void make_row_word_start_mask_returns_no_starts_for_empty_row(void)
{
    assert(ROW_WORD_START_MASK("...............") == UINT16_C(0x0000));
}

static void make_row_word_start_mask_returns_start_for_each_single_letter_position(void)
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
    assert(ROW_WORD_START_MASK("AB..CDE.FG....Z") == UINT16_C(0x4111));
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
    run_test("make_row_packs_blank_tiles_as_all_ones_without_care_masks", make_row_packs_blank_tiles_as_all_ones_without_care_masks);
    run_test("make_row_packs_first_three_tiles_from_left_to_right", make_row_packs_first_three_tiles_from_left_to_right);
    run_test("make_row_packs_last_twelve_tiles_from_left_to_right", make_row_packs_last_twelve_tiles_from_left_to_right);
    run_test("make_row_keeps_blank_tiles_uncared_between_letters", make_row_keeps_blank_tiles_uncared_between_letters);
    run_test("make_row_treats_lowercase_letters_as_letters", make_row_treats_lowercase_letters_as_letters);
    run_test("make_word_start_mask_returns_no_starts_for_empty_row", make_word_start_mask_returns_no_starts_for_empty_row);
    run_test("make_word_start_mask_returns_start_for_each_single_letter_position", make_word_start_mask_returns_start_for_each_single_letter_position);
    run_test("make_word_start_mask_returns_contiguous_group_starts", make_word_start_mask_returns_contiguous_group_starts);
    run_test("make_word_start_mask_handles_boundary_between_tile_groups", make_word_start_mask_handles_boundary_between_tile_groups);
    run_test("make_row_word_start_mask_returns_no_starts_for_empty_row", make_row_word_start_mask_returns_no_starts_for_empty_row);
    run_test("make_row_word_start_mask_returns_start_for_each_single_letter_position", make_row_word_start_mask_returns_start_for_each_single_letter_position);
    run_test("make_row_word_start_mask_returns_contiguous_group_starts", make_row_word_start_mask_returns_contiguous_group_starts);
    run_test("make_row_word_start_mask_handles_boundary_between_tile_groups", make_row_word_start_mask_handles_boundary_between_tile_groups);

    return 0;
}
