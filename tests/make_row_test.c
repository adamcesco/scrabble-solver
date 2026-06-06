#include "board.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define TILE_MASK UINT64_C(0x1F)

static uint64_t packed_tile(char tile, int shift)
{
    return ((uint64_t)(tile & TILE_MASK)) << shift;
}

static void make_row_packs_blank_tiles_as_all_ones_without_care_masks(void)
{
    Row row = make_row("...............");

    assert(row.first3Tiles == UINT16_MAX);
    assert(row.first3CareMask == 0);
    assert(row.last12Tiles == UINT64_MAX);
    assert(row.last12CareMask == 0);
}

static void make_row_packs_first_three_tiles_from_left_to_right(void)
{
    Row row = make_row("ABC............");

    assert(row.first3Tiles == UINT16_C(0x8443));
    assert(row.first3CareMask == UINT16_C(0x7FFF));
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
    assert(row.last12Tiles == expected_tiles);
    assert(row.last12CareMask == UINT64_C(0x0FFFFFFFFFFFFFFF));
}

static void make_row_keeps_blank_tiles_uncared_between_letters(void)
{
    Row row = make_row(".Z.A...........");
    uint64_t expected_last12_tiles = (UINT64_MAX & ~(TILE_MASK << 55)) | packed_tile('A', 55);

    assert(row.first3Tiles == UINT16_C(0xFF5F));
    assert(row.first3CareMask == UINT16_C(0x03E0));
    assert(row.last12Tiles == expected_last12_tiles);
    assert(row.last12CareMask == UINT64_C(0x0F80000000000000));
}

static void make_row_treats_lowercase_letters_as_letters(void)
{
    Row row = make_row("aBc............");

    assert(row.first3Tiles == UINT16_C(0x8443));
    assert(row.first3CareMask == UINT16_C(0x7FFF));
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

    return 0;
}
