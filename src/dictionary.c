#define _POSIX_C_SOURCE 200809L

#include "dictionary.h"

#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int active_hash_table = 0;

static void trim_line_ending(char *line)
{
    line[strcspn(line, "\r\n")] = '\0';
}

static char *copy_word(const char *word)
{
    size_t length = strlen(word) + 1;
    char *copy = malloc(length);

    if (copy != NULL) {
        memcpy(copy, word, length);
    }

    return copy;
}

static size_t count_words(FILE *file)
{
    char *line = NULL;
    size_t line_capacity = 0;
    size_t word_count = 0;

    while (getline(&line, &line_capacity, file) != -1) {
        trim_line_ending(line);

        if (line[0] != '\0') {
            word_count++;
        }
    }

    free(line);
    rewind(file);

    return word_count;
}

static void free_words(char **words, size_t count)
{
    for (size_t word_index = 0; word_index < count; ++word_index) {
        free(words[word_index]);
    }

    free(words);
}

static int add_unique_word(WordTable *table, const char *line)
{
    ENTRY item;
    ENTRY *found;
    char *word;

    item.key = (char *)line;
    item.data = NULL;
    if (hsearch(item, FIND) != NULL) {
        return 1;
    }

    word = copy_word(line);
    if (word == NULL) {
        return 0;
    }

    item.key = word;
    item.data = word;
    found = hsearch(item, ENTER);
    if (found == NULL) {
        free(word);
        return 0;
    }

    table->words[table->count] = word;
    table->count++;

    return 1;
}

WordTable words_from_file(const char *file_path)
{
    WordTable table = {0};
    FILE *file = fopen(file_path, "r");
    size_t max_word_count;
    char *line = NULL;
    size_t line_capacity = 0;

    if (file == NULL) {
        return table;
    }

    max_word_count = count_words(file);
    if (max_word_count == 0) {
        fclose(file);
        return table;
    }

    table.words = calloc(max_word_count, sizeof(table.words[0]));
    if (table.words == NULL) {
        fclose(file);
        return table;
    }

    if (!hcreate(max_word_count + (max_word_count / 4) + 1)) {
        free(table.words);
        fclose(file);
        return (WordTable){0};
    }
    active_hash_table = 1;

    while (getline(&line, &line_capacity, file) != -1) {
        trim_line_ending(line);
        if (line[0] == '\0') {
            continue;
        }

        if (!add_unique_word(&table, line)) {
            word_table_destroy(&table);
            free(line);
            fclose(file);
            return (WordTable){0};
        }
    }

    free(line);
    fclose(file);

    return table;
}

int word_table_contains(const WordTable *table, const char *word)
{
    ENTRY item;

    if (table == NULL || table->words == NULL || !active_hash_table) {
        return 0;
    }

    item.key = (char *)word;
    item.data = NULL;

    return hsearch(item, FIND) != NULL;
}

void word_table_destroy(WordTable *table)
{
    if (table == NULL) {
        return;
    }

    if (active_hash_table) {
        hdestroy();
        active_hash_table = 0;
    }

    free_words(table->words, table->count);
    table->words = NULL;
    table->count = 0;
}
