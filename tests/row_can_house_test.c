#include "board.h"

#include <assert.h>
#include <stdio.h>

static void row_can_house_when_existing_letters_match_proposed_letters(void)
{
    Row board_row = make_row(".......OR......");
    Row proposed_row = make_row("......WORD.....");

    assert(row_can_house(board_row, proposed_row));
}

static void row_can_house_when_board_letters_do_not_overlap_proposed_letters(void)
{
    Row board_row = make_row("AB.............");
    Row proposed_row = make_row("......WORD.....");

    assert(row_can_house(board_row, proposed_row));
}

static void row_can_house_when_board_letters_do_and_do_not_overlap_proposed_letters(void)
{
    Row board_row = make_row("AB.....OR......");
    Row proposed_row = make_row("......WORD.....");

    assert(row_can_house(board_row, proposed_row));
}

static void row_cannot_house_when_existing_letter_conflicts_with_proposed_letter(void)
{
    Row board_row = make_row(".......AR......");
    Row proposed_row = make_row("......WORD.....");

    assert(!row_can_house(board_row, proposed_row));
}

static void row_cannot_house_when_first_three_tiles_conflict(void)
{
    Row board_row = make_row(".A.............");
    Row proposed_row = make_row(".B.............");

    assert(!row_can_house(board_row, proposed_row));
}

static void run_test(const char *name, void (*test)(void))
{
    test();
    printf("PASS %s\n", name);
}

int main(void)
{
    run_test("row_can_house_when_existing_letters_match_proposed_letters", row_can_house_when_existing_letters_match_proposed_letters);
    run_test("row_can_house_when_board_letters_do_not_overlap_proposed_letters", row_can_house_when_board_letters_do_not_overlap_proposed_letters);
    run_test("row_can_house_when_board_letters_do_and_do_not_overlap_proposed_letters", row_can_house_when_board_letters_do_and_do_not_overlap_proposed_letters);
    run_test("row_cannot_house_when_existing_letter_conflicts_with_proposed_letter", row_cannot_house_when_existing_letter_conflicts_with_proposed_letter);
    run_test("row_cannot_house_when_first_three_tiles_conflict", row_cannot_house_when_first_three_tiles_conflict);

    return 0;
}
