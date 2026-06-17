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

    table = WordTable::from_file(temp_dictionary_path);

    assert(table.count == 3);
    assert(table.contains("APPLE"));
    assert(table.contains("BERRY"));
    assert(table.contains("CHERRY"));
    assert(!table.contains("DATE"));

    table.destroy();
}

static void ignores_empty_lines_and_duplicate_words(void)
{
    WordTable table;

    write_text_file("\nAPPLE\n\nAPPLE\nBERRY\n");

    table = WordTable::from_file(temp_dictionary_path);

    assert(table.count == 2);
    assert(table.contains("APPLE"));
    assert(table.contains("BERRY"));

    table.destroy();
}

static void uppercases_alpha_characters_when_loading_words(void)
{
    WordTable table;

    write_text_file("apple\nbErry\nCHERRY\n");

    table = WordTable::from_file(temp_dictionary_path);

    assert(table.count == 3);
    assert(table.contains("APPLE"));
    assert(table.contains("BERRY"));
    assert(table.contains("CHERRY"));
    assert(!table.contains("apple"));

    table.destroy();
}

static void returns_empty_table_when_file_cannot_be_opened(void)
{
    WordTable table = WordTable::from_file("/tmp/scrabble_dictionary_missing.txt");

    assert(table.words == NULL);
    assert(table.count == 0);
    assert(!table.contains("APPLE"));
}

static void searches_multiple_tables_independently(void)
{
    WordTable first_table;
    WordTable second_table;

    write_text_file_at(temp_dictionary_path, "APPLE\nBERRY\n");
    write_text_file_at(second_temp_dictionary_path, "CHERRY\nDATE\n");

    first_table = WordTable::from_file(temp_dictionary_path);
    second_table = WordTable::from_file(second_temp_dictionary_path);

    assert(first_table.count == 2);
    assert(second_table.count == 2);

    assert(first_table.contains("APPLE"));
    assert(first_table.contains("BERRY"));
    assert(!first_table.contains("CHERRY"));
    assert(!first_table.contains("DATE"));

    assert(!second_table.contains("APPLE"));
    assert(!second_table.contains("BERRY"));
    assert(second_table.contains("CHERRY"));
    assert(second_table.contains("DATE"));

    first_table.destroy();

    assert(second_table.contains("CHERRY"));
    assert(second_table.contains("DATE"));

    second_table.destroy();
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
    run_test("uppercases_alpha_characters_when_loading_words", uppercases_alpha_characters_when_loading_words);
    run_test("returns_empty_table_when_file_cannot_be_opened", returns_empty_table_when_file_cannot_be_opened);
    run_test("searches_multiple_tables_independently", searches_multiple_tables_independently);

    return 0;
}
