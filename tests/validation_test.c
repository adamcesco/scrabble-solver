#include "validation.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

static char temp_dictionary_path[128];
static WordTable dictionary;
static uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1];

static void setup(void)
{
    dictionary = (WordTable){0};
    snprintf(temp_dictionary_path, sizeof(temp_dictionary_path), "/tmp/scrabble_validation_%ld.txt", (long)getpid());
    init_config_map(config_to_start_positions);
}

static void cleanup_after_each(void)
{
    word_table_destroy(&dictionary);
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
    dictionary = words_from_file(temp_dictionary_path);
}

static void accepts_valid_perpendicular_words_in_played_span(void)
{
    Board old_board = {0};
    Board board = {0};

    old_board.perpendicularRows[5] = make_row("......T........");
    old_board.perpendicularRows[6] = make_row("......O........");
    board.perpendicularRows[5] = make_row(".....AT........");
    board.perpendicularRows[6] = make_row(".....TO........");

    load_dictionary("AT\nTO\n");

    assert(validate_perpendicular_rows(&dictionary, config_to_start_positions, &board, &old_board, 5, 2, 5));
}

static void rejects_invalid_perpendicular_word_in_played_span(void)
{
    Board old_board = {0};
    Board board = {0};

    old_board.perpendicularRows[5] = make_row("......T........");
    old_board.perpendicularRows[6] = make_row("......Z........");
    board.perpendicularRows[5] = make_row(".....AT........");
    board.perpendicularRows[6] = make_row(".....ZZ........");

    load_dictionary("AT\nTO\n");

    assert(!validate_perpendicular_rows(&dictionary, config_to_start_positions, &board, &old_board, 5, 2, 5));
}

static void rejects_dictionary_words_that_extend_past_actual_perpendicular_word(void)
{
    Board old_board = {0};
    Board board = {0};

    old_board.perpendicularRows[7] = make_row(".....CA........");
    board.perpendicularRows[7] = make_row(".....CAT.......");

    load_dictionary("CATS\n");

    assert(!validate_perpendicular_rows(&dictionary, config_to_start_positions, &board, &old_board, 7, 1, 7));
}

static void ignores_single_tile_perpendicular_intersections(void)
{
    Board old_board = {0};
    Board board = {0};

    board.perpendicularRows[7] = make_row("......A........");

    load_dictionary("CAT\n");

    assert(validate_perpendicular_rows(&dictionary, config_to_start_positions, &board, &old_board, 7, 1, 6));
}

static void ignores_perpendicular_words_outside_played_span(void)
{
    Board old_board = {0};
    Board board = {0};

    old_board.perpendicularRows[5] = make_row("......T........");
    old_board.perpendicularRows[6] = make_row("......O........");
    board.perpendicularRows[4] = make_row(".....ZZ........");
    board.perpendicularRows[5] = make_row(".....AT........");
    board.perpendicularRows[6] = make_row(".....TO........");
    board.perpendicularRows[7] = make_row(".....ZZ........");

    load_dictionary("AT\nTO\n");

    assert(validate_perpendicular_rows(&dictionary, config_to_start_positions, &board, &old_board, 5, 2, 5));
}

static void validates_every_word_start_in_each_perpendicular_row(void)
{
    Board old_board = {0};
    Board board = {0};

    old_board.perpendicularRows[5] = make_row(".T...N.........");
    board.perpendicularRows[5] = make_row("AT...IN........");

    load_dictionary("AT\n");

    assert(!validate_perpendicular_rows(&dictionary, config_to_start_positions, &board, &old_board, 5, 1, 5));
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
