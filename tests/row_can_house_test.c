#include "board.h"

#include <assert.h>
#include <stdio.h>

static void row_is_placeable_when_existing_letters_match_proposed_letters(void)
{
    Row board_row = make_row(".......OR......");
    Row proposed_row = make_row("......WORD.....");

    assert(is_placeable_on_row(&board_row, &proposed_row));
}

static void row_is_not_placeable_when_board_letters_do_not_overlap_proposed_letters(void)
{
    Row board_row = make_row("AB.............");
    Row proposed_row = make_row("......WORD.....");

    assert(!is_placeable_on_row(&board_row, &proposed_row));
}

static void row_is_placeable_when_board_letters_do_and_do_not_overlap_proposed_letters(void)
{
    Row board_row = make_row("AB.....OR......");
    Row proposed_row = make_row("......WORD.....");

    assert(is_placeable_on_row(&board_row, &proposed_row));
}

static void row_is_not_placeable_when_existing_letter_conflicts_with_proposed_letter(void)
{
    Row board_row = make_row(".......AR......");
    Row proposed_row = make_row("......WORD.....");

    assert(!is_placeable_on_row(&board_row, &proposed_row));
}

static void row_is_not_placeable_when_existing_letter_conflicts_with_two_letter_proposed_word(void)
{
    Row board_row = make_row(".A.............");
    Row proposed_row = make_row(".BC............");

    assert(!is_placeable_on_row(&board_row, &proposed_row));
}

static void row_is_not_placeable_when_tile_before_proposed_word_is_occupied(void)
{
    Row board_row = make_row(".....A.........");
    Row proposed_row = make_row("......WORD.....");

    assert(!is_placeable_on_row(&board_row, &proposed_row));
}

static void row_is_not_placeable_when_tile_after_proposed_word_is_occupied(void)
{
    Row board_row = make_row("..........A....");
    Row proposed_row = make_row("......WORD.....");

    assert(!is_placeable_on_row(&board_row, &proposed_row));
}

static void row_is_placeable_when_proposed_word_starts_at_beginning_of_row(void)
{
    Row board_row = make_row("T..............");
    Row proposed_row = make_row("TO.............");

    assert(is_placeable_on_row(&board_row, &proposed_row));
}

static void row_is_placeable_when_proposed_word_ends_at_end_of_row(void)
{
    Row board_row = make_row("..............O");
    Row proposed_row = make_row(".............TO");

    assert(is_placeable_on_row(&board_row, &proposed_row));
}

static void add_proposed_word_to_row_returns_new_row_with_existing_and_proposed_tiles(void)
{
    Row board_row = make_row("AB.....OR......");
    Row proposed_row = make_row("......WORD.....");
    Row expected_row = make_row("AB....WORD.....");
    Row row;

    add_proposed_word_to_row(&row, &board_row, &proposed_row);

    assert(row.tiles == expected_row.tiles);
    assert(row.careMask == expected_row.careMask);
    assert(row.occupiedMask == expected_row.occupiedMask);
}

static Board make_empty_board(void)
{
    Board board;
    Row empty_row = make_row("...............");

    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        board.rows[row_index] = empty_row;
        board.perpendicularRows[row_index] = empty_row;
    }

    return board;
}

static void place_word_row_on_board_overwrites_row_and_updates_perpendicular_rows(void)
{
    Board board = make_empty_board();
    Row row = make_row("......WORD.....");
    place_row_with_new_word_on_board(&board, &row, 3, 6, 4);

    assert(board.rows[3].tiles == row.tiles);
    assert(board.rows[3].careMask == row.careMask);
    assert(board.rows[3].occupiedMask == row.occupiedMask);

    Row expected_w_col = make_row("...W...........");
    Row expected_o_col = make_row("...O...........");
    Row expected_r_col = make_row("...R...........");
    Row expected_d_col = make_row("...D...........");

    assert(board.perpendicularRows[6].tiles == expected_w_col.tiles);
    assert(board.perpendicularRows[6].careMask == expected_w_col.careMask);
    assert(board.perpendicularRows[6].occupiedMask == expected_w_col.occupiedMask);
    assert(board.perpendicularRows[7].tiles == expected_o_col.tiles);
    assert(board.perpendicularRows[7].careMask == expected_o_col.careMask);
    assert(board.perpendicularRows[7].occupiedMask == expected_o_col.occupiedMask);
    assert(board.perpendicularRows[8].tiles == expected_r_col.tiles);
    assert(board.perpendicularRows[8].careMask == expected_r_col.careMask);
    assert(board.perpendicularRows[8].occupiedMask == expected_r_col.occupiedMask);
    assert(board.perpendicularRows[9].tiles == expected_d_col.tiles);
    assert(board.perpendicularRows[9].careMask == expected_d_col.careMask);
    assert(board.perpendicularRows[9].occupiedMask == expected_d_col.occupiedMask);
}

static void place_word_row_on_board_leaves_perpendicular_rows_outside_word_unchanged(void)
{
    Board board = make_empty_board();
    Row original_perpendicular_row = make_row("A..............");
    Row row = make_row("......WORD.....");

    board.perpendicularRows[5] = original_perpendicular_row;
    board.perpendicularRows[10] = original_perpendicular_row;

    place_row_with_new_word_on_board(&board, &row, 3, 6, 4);

    assert(board.perpendicularRows[5].tiles == original_perpendicular_row.tiles);
    assert(board.perpendicularRows[5].careMask == original_perpendicular_row.careMask);
    assert(board.perpendicularRows[5].occupiedMask == original_perpendicular_row.occupiedMask);
    assert(board.perpendicularRows[10].tiles == original_perpendicular_row.tiles);
    assert(board.perpendicularRows[10].careMask == original_perpendicular_row.careMask);
    assert(board.perpendicularRows[10].occupiedMask == original_perpendicular_row.occupiedMask);
}

static void run_test(const char *name, void (*test)(void))
{
    test();
    printf("PASS %s\n", name);
}

int main(void)
{
    run_test("row_is_placeable_when_existing_letters_match_proposed_letters", row_is_placeable_when_existing_letters_match_proposed_letters);
    run_test("row_is_not_placeable_when_board_letters_do_not_overlap_proposed_letters", row_is_not_placeable_when_board_letters_do_not_overlap_proposed_letters);
    run_test("row_is_placeable_when_board_letters_do_and_do_not_overlap_proposed_letters", row_is_placeable_when_board_letters_do_and_do_not_overlap_proposed_letters);
    run_test("row_is_not_placeable_when_existing_letter_conflicts_with_proposed_letter", row_is_not_placeable_when_existing_letter_conflicts_with_proposed_letter);
    run_test("row_is_not_placeable_when_existing_letter_conflicts_with_two_letter_proposed_word", row_is_not_placeable_when_existing_letter_conflicts_with_two_letter_proposed_word);
    run_test("row_is_not_placeable_when_tile_before_proposed_word_is_occupied", row_is_not_placeable_when_tile_before_proposed_word_is_occupied);
    run_test("row_is_not_placeable_when_tile_after_proposed_word_is_occupied", row_is_not_placeable_when_tile_after_proposed_word_is_occupied);
    run_test("row_is_placeable_when_proposed_word_starts_at_beginning_of_row", row_is_placeable_when_proposed_word_starts_at_beginning_of_row);
    run_test("row_is_placeable_when_proposed_word_ends_at_end_of_row", row_is_placeable_when_proposed_word_ends_at_end_of_row);
    run_test("add_proposed_word_to_row_returns_new_row_with_existing_and_proposed_tiles", add_proposed_word_to_row_returns_new_row_with_existing_and_proposed_tiles);
    run_test("place_word_row_on_board_overwrites_row_and_updates_perpendicular_rows", place_word_row_on_board_overwrites_row_and_updates_perpendicular_rows);
    run_test("place_word_row_on_board_leaves_perpendicular_rows_outside_word_unchanged", place_word_row_on_board_leaves_perpendicular_rows_outside_word_unchanged);

    return 0;
}
