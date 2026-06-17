#include "rack_oriented_solver.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

static char temp_dictionary_path[128];

static void setup(void)
{
    snprintf(temp_dictionary_path, sizeof(temp_dictionary_path), "/tmp/scrabble_rack_solver_%ld.txt", (long)getpid());
}

static void cleanup_after_each(void)
{
    remove(temp_dictionary_path);
}

static void write_text_file(const char *contents)
{
    FILE *file = fopen(temp_dictionary_path, "w");
    assert(file != NULL);
    fputs(contents, file);
    fclose(file);
}

static int solve_with_dictionary(Board *board, const char *dictionary_contents, const char *rack)
{
    write_text_file(dictionary_contents);

    WordTable dictionary = WordTable::from_file(temp_dictionary_path);
    WordPatternTable patterns = WordPatternTable::from_words(dictionary);
    WordIndexStartRowTable rows = WordIndexStartRowTable::from_words(dictionary);
    uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1];
    init_config_map(config_to_start_positions);
    int count = rack_oriented_solver(&dictionary, &patterns, &rows, config_to_start_positions, board, rack);

    rows.destroy();
    dictionary.destroy();

    return count;
}

static void counts_words_matching_board_pattern_and_rack_subset(void)
{
    Board board = {};

    board.rows[7] = make_row(".......A.......");
    board.perpendicularRows[7] = make_row(".......A.......");

    assert(solve_with_dictionary(&board, "CAT\nDOG\n", "CT") == 1);
    assert(board.rows[7].tiles == make_row(".......A.......").tiles);
    assert(board.perpendicularRows[7].tiles == make_row(".......A.......").tiles);
}

static void rejects_candidates_with_invalid_perpendicular_words(void)
{
    Board board = {};

    board.rows[7] = make_row(".......A.......");
    board.rows[8] = make_row("......T........");
    board.perpendicularRows[6] = make_row("........T......");
    board.perpendicularRows[7] = make_row(".......A.......");

    assert(solve_with_dictionary(&board, "CAT\n", "CT") == 0);
}

static void rejects_windows_with_blank_only_patterns(void)
{
    Board board = {};

    assert(solve_with_dictionary(&board, "CAT\n", "ACT") == 0);
}

static void allows_blank_only_window_touching_neighboring_row(void)
{
    Board board = {};

    board.rows[6] = make_row(".......X.......");
    board.perpendicularRows[7] = make_row("......X........");

    assert(solve_with_dictionary(&board, "CAT\nXA\n", "ACT") > 0);
}

static void allows_blank_only_window_using_letters_after_matching_rack_length_prefix(void)
{
    Board board = {};

    board.rows[6] = make_row(".......X.......");
    board.perpendicularRows[7] = make_row("......X........");

    assert(solve_with_dictionary(&board, "CHAI\nXA\nXC\nXH\nXI\n", "ACFHIIU") > 0);
}

static void rejects_windows_that_start_inside_existing_row_word(void)
{
    Board board = {};

    board.rows[7] = make_row("XA.............");
    board.perpendicularRows[0] = make_row(".......X.......");
    board.perpendicularRows[1] = make_row(".......A.......");

    assert(solve_with_dictionary(&board, "ACT\n", "CT") == 0);
}

static void rejects_windows_that_end_inside_existing_row_word(void)
{
    Board board = {};

    board.rows[7] = make_row("............AX.");
    board.perpendicularRows[12] = make_row(".......A.......");
    board.perpendicularRows[13] = make_row(".......X.......");

    assert(solve_with_dictionary(&board, "CTA\n", "CT") == 0);
}

static void allows_window_starting_at_left_board_edge(void)
{
    Board board = {};

    board.rows[7] = make_row("CA.............");
    board.perpendicularRows[0] = make_row(".......C.......");
    board.perpendicularRows[1] = make_row(".......A.......");

    assert(solve_with_dictionary(&board, "CAT\n", "T") == 1);
}

static void allows_window_ending_at_right_board_edge(void)
{
    Board board = {};

    board.rows[7] = make_row(".............AT");
    board.perpendicularRows[13] = make_row(".......A.......");
    board.perpendicularRows[14] = make_row(".......T.......");

    assert(solve_with_dictionary(&board, "CAT\n", "C") == 1);
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
    run_test("counts_words_matching_board_pattern_and_rack_subset", counts_words_matching_board_pattern_and_rack_subset);
    run_test("rejects_candidates_with_invalid_perpendicular_words", rejects_candidates_with_invalid_perpendicular_words);
    run_test("rejects_windows_with_blank_only_patterns", rejects_windows_with_blank_only_patterns);
    run_test("allows_blank_only_window_touching_neighboring_row", allows_blank_only_window_touching_neighboring_row);
    run_test("allows_blank_only_window_using_letters_after_matching_rack_length_prefix", allows_blank_only_window_using_letters_after_matching_rack_length_prefix);
    run_test("rejects_windows_that_start_inside_existing_row_word", rejects_windows_that_start_inside_existing_row_word);
    run_test("rejects_windows_that_end_inside_existing_row_word", rejects_windows_that_end_inside_existing_row_word);
    run_test("allows_window_starting_at_left_board_edge", allows_window_starting_at_left_board_edge);
    run_test("allows_window_ending_at_right_board_edge", allows_window_ending_at_right_board_edge);

    return 0;
}
