#include "dictionary.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

static char temp_dictionary_path[128];

static void setup(void)
{
    snprintf(temp_dictionary_path, sizeof(temp_dictionary_path), "/tmp/scrabble_dictionary_%ld.txt", (long)getpid());
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

    return 0;
}
