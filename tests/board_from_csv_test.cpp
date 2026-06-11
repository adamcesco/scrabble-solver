#include "board.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static char temp_board_path[128];

static void setup(void)
{
    snprintf(temp_board_path, sizeof(temp_board_path), "/tmp/scrabble_board_%ld.csv", (long)getpid());

    FILE *file = fopen(temp_board_path, "w");
    assert(file != NULL);
    fclose(file);
}

static void cleanup_after_each(void)
{
    remove(temp_board_path);
}

static void write_csv_row(FILE *file, const char tiles[BOARD_SIZE + 1])
{
    for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
        if (col_index > 0) {
            fputc(',', file);
        }

        fputc(tiles[col_index], file);
    }

    fputc('\n', file);
}

static void write_board_with_first_row(const char first_row[BOARD_SIZE + 1])
{
    FILE *file = fopen(temp_board_path, "w");
    assert(file != NULL);

    write_csv_row(file, first_row);

    for (int row_index = 1; row_index < BOARD_SIZE; ++row_index) {
        write_csv_row(file, "...............");
    }

    fclose(file);
}

static void write_text_file(const char *contents)
{
    FILE *file = fopen(temp_board_path, "w");
    assert(file != NULL);
    fputs(contents, file);
    fclose(file);
}

static void assert_board_is_zeroed(Board board)
{
    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        assert(board.rows[row_index].tiles == 0);
        assert(board.rows[row_index].occupiedMask == 0);
        assert(board.perpendicularRows[row_index].tiles == 0);
        assert(board.perpendicularRows[row_index].occupiedMask == 0);
    }
}

static void write_board_with_diagonal_letters(void)
{
    FILE *file = fopen(temp_board_path, "w");
    assert(file != NULL);

    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        char tiles[BOARD_SIZE + 1] = "...............";
        tiles[row_index] = (char)('A' + row_index);
        write_csv_row(file, tiles);
    }

    fclose(file);
}

static void loads_letters_from_csv_board(void)
{
    const char first_row[BOARD_SIZE + 1] = "ABCDEFGHIJKLMNO";
    Row expected_first_row = make_row(first_row);

    write_board_with_first_row(first_row);

    Board board = board_from_csv(temp_board_path);

    assert(board.rows[0].tiles == expected_first_row.tiles);

    for (int row_index = 1; row_index < BOARD_SIZE; ++row_index) {
        assert(board.rows[row_index].tiles == 0);
    }
}

static void keeps_empty_tiles_as_blank_values(void)
{
    const char first_row[BOARD_SIZE + 1] = ".Z.A...........";
    Row expected_first_row = make_row(first_row);

    write_board_with_first_row(first_row);

    Board board = board_from_csv(temp_board_path);

    assert(board.rows[0].tiles == expected_first_row.tiles);
}

static void loads_perpendicular_rows_from_csv_board(void)
{
    write_board_with_diagonal_letters();

    Board board = board_from_csv(temp_board_path);

    for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
        char expected_tiles[BOARD_SIZE + 1] = "...............";
        expected_tiles[col_index] = (char)('A' + col_index);
        Row expected_row = make_row(expected_tiles);

        assert(board.perpendicularRows[col_index].tiles == expected_row.tiles);
        assert(board.perpendicularRows[col_index].careMask == expected_row.careMask);
        assert(board.perpendicularRows[col_index].occupiedMask == expected_row.occupiedMask);
    }
}

static void returns_zeroed_board_when_first_csv_row_is_malformed(void)
{
    write_text_file("A,B,C\n");

    Board board = board_from_csv(temp_board_path);

    assert_board_is_zeroed(board);
}

static void run_test(const char *name, void (*test)(void))
{
    setup();
    test();
    cleanup_after_each();
    printf("PASS %s\n", name);
}

int main(void)
{
    run_test("loads_letters_from_csv_board", loads_letters_from_csv_board);
    run_test("keeps_empty_tiles_as_blank_values", keeps_empty_tiles_as_blank_values);
    run_test("loads_perpendicular_rows_from_csv_board", loads_perpendicular_rows_from_csv_board);
    run_test("returns_zeroed_board_when_first_csv_row_is_malformed", returns_zeroed_board_when_first_csv_row_is_malformed);

    return 0;
}
