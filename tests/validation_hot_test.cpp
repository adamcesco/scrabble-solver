#include "validation_hot.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

static char temp_dictionary_path[128];
static WordTable dictionary;

static void setup(void)
{
    dictionary = WordTable{};
    snprintf(temp_dictionary_path, sizeof(temp_dictionary_path), "/tmp/scrabble_validation_%ld.txt", (long)getpid());
}

static void cleanup_after_each(void)
{
    dictionary.destroy();
    remove(temp_dictionary_path);
}

static void write_text_file(const char *contents)
{
    FILE *file = fopen(temp_dictionary_path, "w");
    assert(file != NULL);
    fputs(contents, file);
    fclose(file);
}

static void load_dictionary(const char *contents)
{
    write_text_file(contents);
    dictionary = WordTable::from_file(temp_dictionary_path);
}

static int place_and_validate(Board *board, const Board *old_board, const char row_tiles[BOARD_SIZE + 1], uint8_t row_index, uint8_t word_start, uint8_t word_length)
{
    Row row = make_row(row_tiles);
    uint16_t touched_columns = 0;

    return hot_place_word_onto_perpendicular_rows_and_validate_with_touched_mask(
        &dictionary,
        old_board->perpendicularRows,
        board,
        &row,
        row_index,
        word_start,
        word_length,
        &touched_columns
    );
}

static void accepts_valid_perpendicular_words_in_played_span(void)
{
    Board old_board = {};

    old_board.perpendicularRows[5] = make_row("......T........");
    old_board.perpendicularRows[6] = make_row("......O........");
    Board board = old_board;

    load_dictionary("AT\nTO\n");

    assert(place_and_validate(&board, &old_board, ".....AT........", 5, 5, 2));
}

static void rejects_invalid_perpendicular_word_in_played_span(void)
{
    Board old_board = {};

    old_board.perpendicularRows[5] = make_row("......T........");
    old_board.perpendicularRows[6] = make_row("......Z........");
    Board board = old_board;

    load_dictionary("AT\nTO\n");

    assert(!place_and_validate(&board, &old_board, ".....AZ........", 5, 5, 2));
}

static void rejects_dictionary_words_that_extend_past_actual_perpendicular_word(void)
{
    Board old_board = {};

    old_board.perpendicularRows[7] = make_row(".....CA........");
    Board board = old_board;

    load_dictionary("CATS\n");

    assert(!place_and_validate(&board, &old_board, ".......T.......", 7, 7, 1));
}

static void ignores_single_tile_perpendicular_intersections(void)
{
    Board old_board = {};
    Board board = old_board;

    load_dictionary("CAT\n");

    assert(place_and_validate(&board, &old_board, ".......A.......", 6, 7, 1));
}

static void ignores_perpendicular_words_outside_played_span(void)
{
    Board old_board = {};

    old_board.perpendicularRows[5] = make_row("......T........");
    old_board.perpendicularRows[6] = make_row("......O........");
    Board board = old_board;
    board.perpendicularRows[4] = make_row(".....ZZ........");
    board.perpendicularRows[7] = make_row(".....ZZ........");

    load_dictionary("AT\nTO\n");

    assert(place_and_validate(&board, &old_board, ".....AT........", 5, 5, 2));
}

static void validates_every_word_start_in_each_perpendicular_row(void)
{
    Board old_board = {};

    old_board.perpendicularRows[5] = make_row(".T....N........");
    Board board = old_board;

    load_dictionary("AT\n");

    assert(!place_and_validate(&board, &old_board, ".....I.........", 5, 5, 1));
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
    run_test("accepts_valid_perpendicular_words_in_played_span", accepts_valid_perpendicular_words_in_played_span);
    run_test("rejects_invalid_perpendicular_word_in_played_span", rejects_invalid_perpendicular_word_in_played_span);
    run_test("rejects_dictionary_words_that_extend_past_actual_perpendicular_word", rejects_dictionary_words_that_extend_past_actual_perpendicular_word);
    run_test("ignores_single_tile_perpendicular_intersections", ignores_single_tile_perpendicular_intersections);
    run_test("ignores_perpendicular_words_outside_played_span", ignores_perpendicular_words_outside_played_span);
    run_test("validates_every_word_start_in_each_perpendicular_row", validates_every_word_start_in_each_perpendicular_row);

    return 0;
}
