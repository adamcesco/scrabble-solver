#include "dictionary.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

static char temp_dictionary_path[128];
static char second_temp_dictionary_path[128];

static void setup(void)
{
    snprintf(temp_dictionary_path, sizeof(temp_dictionary_path), "/tmp/scrabble_dictionary_%ld.txt", (long)getpid());
    snprintf(second_temp_dictionary_path, sizeof(second_temp_dictionary_path), "/tmp/scrabble_dictionary_second_%ld.txt", (long)getpid());
}

static void cleanup_after_each(void)
{
    remove(temp_dictionary_path);
    remove(second_temp_dictionary_path);
}

static void write_text_file_at(const char *file_path, const char *contents)
{
    FILE *file = fopen(file_path, "w");
    assert(file != NULL);
    fputs(contents, file);
    fclose(file);
}

static void write_text_file(const char *contents)
{
    write_text_file_at(temp_dictionary_path, contents);
}

static void loads_one_word_per_line_into_hash_table(void)
{
    WordTable table;

    write_text_file("APPLE\nBERRY\nCHERRY\n");

    table = words_from_file(temp_dictionary_path);

    assert(table.count == 3);
    assert(word_table_contains(&table, "APPLE"));
    assert(word_table_contains(&table, "BERRY"));
    assert(word_table_contains(&table, "CHERRY"));
    assert(!word_table_contains(&table, "DATE"));

    word_table_destroy(&table);
}

static void ignores_empty_lines_and_duplicate_words(void)
{
    WordTable table;

    write_text_file("\nAPPLE\n\nAPPLE\nBERRY\n");

    table = words_from_file(temp_dictionary_path);

    assert(table.count == 2);
    assert(word_table_contains(&table, "APPLE"));
    assert(word_table_contains(&table, "BERRY"));

    word_table_destroy(&table);
}

static void returns_empty_table_when_file_cannot_be_opened(void)
{
    WordTable table = words_from_file("/tmp/scrabble_dictionary_missing.txt");

    assert(table.words == NULL);
    assert(table.count == 0);
    assert(!word_table_contains(&table, "APPLE"));
}

static void searches_multiple_tables_independently(void)
{
    WordTable first_table;
    WordTable second_table;

    write_text_file_at(temp_dictionary_path, "APPLE\nBERRY\n");
    write_text_file_at(second_temp_dictionary_path, "CHERRY\nDATE\n");

    first_table = words_from_file(temp_dictionary_path);
    second_table = words_from_file(second_temp_dictionary_path);

    assert(first_table.count == 2);
    assert(second_table.count == 2);

    assert(word_table_contains(&first_table, "APPLE"));
    assert(word_table_contains(&first_table, "BERRY"));
    assert(!word_table_contains(&first_table, "CHERRY"));
    assert(!word_table_contains(&first_table, "DATE"));

    assert(!word_table_contains(&second_table, "APPLE"));
    assert(!word_table_contains(&second_table, "BERRY"));
    assert(word_table_contains(&second_table, "CHERRY"));
    assert(word_table_contains(&second_table, "DATE"));

    word_table_destroy(&first_table);

    assert(word_table_contains(&second_table, "CHERRY"));
    assert(word_table_contains(&second_table, "DATE"));

    word_table_destroy(&second_table);
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
    run_test("loads_one_word_per_line_into_hash_table", loads_one_word_per_line_into_hash_table);
    run_test("ignores_empty_lines_and_duplicate_words", ignores_empty_lines_and_duplicate_words);
    run_test("returns_empty_table_when_file_cannot_be_opened", returns_empty_table_when_file_cannot_be_opened);
    run_test("searches_multiple_tables_independently", searches_multiple_tables_independently);

    return 0;
}
