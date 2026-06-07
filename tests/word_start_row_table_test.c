#include "dictionary.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

static char temp_dictionary_path[128];

static void setup(void)
{
    snprintf(temp_dictionary_path, sizeof(temp_dictionary_path), "/tmp/scrabble_word_start_rows_%ld.txt", (long)getpid());
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

static void assert_rows_equal(Row actual, Row expected)
{
    assert(actual.first3Tiles == expected.first3Tiles);
    assert(actual.first3CareMask == expected.first3CareMask);
    assert(actual.occupiedMask == expected.occupiedMask);
    assert(actual.last12Tiles == expected.last12Tiles);
    assert(actual.last12CareMask == expected.last12CareMask);
}

static void builds_rows_for_words_at_requested_start_positions(void)
{
    WordTable words;
    WordStartRowTable rows;
    const Row *cat_at_two;
    const Row *cat_at_twelve;

    write_text_file("CAT\nBERRY\n");

    words = words_from_file(temp_dictionary_path);
    rows = word_start_row_table_from_word_table(&words);

    assert(rows.count == 2);

    cat_at_two = word_start_row_table_get(&rows, "CAT", 2);
    cat_at_twelve = word_start_row_table_get(&rows, "CAT", 12);

    assert(cat_at_two != NULL);
    assert(cat_at_twelve != NULL);
    assert_rows_equal(*cat_at_two, make_row("..CAT.........."));
    assert_rows_equal(*cat_at_twelve, make_row("............CAT"));

    word_start_row_table_destroy(&rows);
    word_table_destroy(&words);
}

static void returns_null_for_unknown_words_or_words_that_do_not_fit(void)
{
    WordTable words;
    WordStartRowTable rows;

    write_text_file("CAT\nBERRY\n");

    words = words_from_file(temp_dictionary_path);
    rows = word_start_row_table_from_word_table(&words);

    assert(word_start_row_table_get(&rows, "DOG", 0) == NULL);
    assert(word_start_row_table_get(&rows, "CAT", BOARD_SIZE) == NULL);
    assert(word_start_row_table_get(&rows, "CAT", 13) == NULL);
    assert(word_start_row_table_get(&rows, "BERRY", 11) == NULL);

    word_start_row_table_destroy(&rows);
    word_table_destroy(&words);
}

static void returns_empty_table_for_empty_or_null_word_tables(void)
{
    WordTable words = {0};
    WordStartRowTable rows = word_start_row_table_from_word_table(&words);
    WordStartRowTable null_rows = word_start_row_table_from_word_table(NULL);

    assert(rows.entries == NULL);
    assert(rows.count == 0);
    assert(word_start_row_table_get(&rows, "CAT", 0) == NULL);

    assert(null_rows.entries == NULL);
    assert(null_rows.count == 0);
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
    run_test("builds_rows_for_words_at_requested_start_positions", builds_rows_for_words_at_requested_start_positions);
    run_test("returns_null_for_unknown_words_or_words_that_do_not_fit", returns_null_for_unknown_words_or_words_that_do_not_fit);
    run_test("returns_empty_table_for_empty_or_null_word_tables", returns_empty_table_for_empty_or_null_word_tables);

    return 0;
}
