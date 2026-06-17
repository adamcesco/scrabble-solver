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
    assert(actual.tiles == expected.tiles);
    assert(actual.careMask == expected.careMask);
    assert(actual.occupiedMask == expected.occupiedMask);
}

static void builds_rows_for_words_at_requested_start_positions(void)
{
    WordTable words;
    WordStartRowTable rows;
    const Row *cat_at_two;
    const Row *cat_at_twelve;

    write_text_file("CAT\nBERRY\n");

    words = WordTable::from_file(temp_dictionary_path);
    rows = WordStartRowTable::from_words(words);

    assert(rows.count == 2);
    assert(rows.entries[0].word_length == 3);
    assert(rows.entries[1].word_length == 5);

    cat_at_two = rows.get("CAT", 2);
    cat_at_twelve = rows.get("CAT", 12);

    assert(cat_at_two != NULL);
    assert(cat_at_twelve != NULL);
    assert_rows_equal(*cat_at_two, make_row("..CAT.........."));
    assert_rows_equal(*cat_at_twelve, make_row("............CAT"));

    rows.destroy();
    words.destroy();
}

static void returns_null_for_unknown_words_or_words_that_do_not_fit(void)
{
    WordTable words;
    WordStartRowTable rows;

    write_text_file("CAT\nBERRY\n");

    words = WordTable::from_file(temp_dictionary_path);
    rows = WordStartRowTable::from_words(words);

    assert(rows.get("DOG", 0) == NULL);
    assert(rows.get("CAT", BOARD_SIZE) == NULL);
    assert(rows.get("CAT", 13) == NULL);
    assert(rows.get("BERRY", 11) == NULL);

    rows.destroy();
    words.destroy();
}

static void returns_empty_table_for_empty_word_tables(void)
{
    WordTable words = {};
    WordStartRowTable rows = WordStartRowTable::from_words(words);

    assert(rows.entries == NULL);
    assert(rows.count == 0);
    assert(rows.get("CAT", 0) == NULL);
}

static void builds_indexed_rows_for_word_indices_at_requested_start_positions(void)
{
    WordTable words;
    WordIndexStartRowTable rows;
    const Row *cat_at_two;
    const Row *berry_at_ten;

    write_text_file("CAT\nBERRY\n");

    words = WordTable::from_file(temp_dictionary_path);
    rows = WordIndexStartRowTable::from_words(words);

    assert(rows.count == 2);
    assert(rows.entries[0].word_length == 3);
    assert(rows.entries[1].word_length == 5);

    cat_at_two = rows.get(0, 2);
    berry_at_ten = rows.get(1, 10);

    assert(cat_at_two != NULL);
    assert(berry_at_ten != NULL);
    assert_rows_equal(*cat_at_two, make_row("..CAT.........."));
    assert_rows_equal(*berry_at_ten, make_row("..........BERRY"));

    rows.destroy();
    words.destroy();
}

static void indexed_rows_return_null_for_unknown_indices_or_words_that_do_not_fit(void)
{
    WordTable words;
    WordIndexStartRowTable rows;

    write_text_file("CAT\nBERRY\n");

    words = WordTable::from_file(temp_dictionary_path);
    rows = WordIndexStartRowTable::from_words(words);

    assert(rows.get(2, 0) == NULL);
    assert(rows.get(0, BOARD_SIZE) == NULL);
    assert(rows.get(0, 13) == NULL);
    assert(rows.get(1, 11) == NULL);

    rows.destroy();
    words.destroy();
}

static void indexed_rows_return_empty_table_for_empty_word_tables(void)
{
    WordTable words = {};
    WordIndexStartRowTable rows = WordIndexStartRowTable::from_words(words);

    assert(rows.entries == NULL);
    assert(rows.count == 0);
    assert(rows.get(0, 0) == NULL);

    rows.destroy();
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
    run_test("returns_empty_table_for_empty_word_tables", returns_empty_table_for_empty_word_tables);
    run_test("builds_indexed_rows_for_word_indices_at_requested_start_positions", builds_indexed_rows_for_word_indices_at_requested_start_positions);
    run_test("indexed_rows_return_null_for_unknown_indices_or_words_that_do_not_fit", indexed_rows_return_null_for_unknown_indices_or_words_that_do_not_fit);
    run_test("indexed_rows_return_empty_table_for_empty_word_tables", indexed_rows_return_empty_table_for_empty_word_tables);

    return 0;
}
