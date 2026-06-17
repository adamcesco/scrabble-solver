#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "word_table.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim_line_ending(char *line)
{
    line[strcspn(line, "\r\n")] = '\0';
}

static void uppercase_alpha_chars(char *word)
{
    for (size_t i = 0; word[i] != '\0'; ++i) {
        unsigned char ch = (unsigned char)word[i];
        if (isalpha(ch)) {
            word[i] = (char)toupper(ch);
        }
    }
}

static char *copy_word(const char *word)
{
    size_t length = strlen(word) + 1;
    char *copy = static_cast<char *>(malloc(length));

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
    if (table->word_set.find(line) != table->word_set.end()) {
        return 1;
    }

    char *word = copy_word(line);
    if (word == NULL) {
        return 0;
    }

    try {
        table->word_set.insert(word);
    } catch (...) {
        free(word);
        return 0;
    }

    table->words[table->count++] = word;
    return 1;
}

WordTable WordTable::from_file(const char *file_path)
{
    WordTable table = {};
    FILE *file = fopen(file_path, "r");
    char *line = NULL;
    size_t line_capacity = 0;

    if (file == NULL) {
        return table;
    }

    size_t max_word_count = count_words(file);
    if (max_word_count == 0) {
        fclose(file);
        return table;
    }

    table.words = static_cast<char **>(calloc(max_word_count, sizeof(table.words[0])));
    if (table.words == NULL) {
        fclose(file);
        return table;
    }

    try {
        table.word_set.reserve(max_word_count + (max_word_count / 4) + 1);
    } catch (...) {
        free(table.words);
        fclose(file);
        return WordTable{};
    }

    while (getline(&line, &line_capacity, file) != -1) {
        trim_line_ending(line);
        uppercase_alpha_chars(line);
        if (line[0] == '\0') {
            continue;
        }

        if (!add_unique_word(&table, line)) {
            table.destroy();
            free(line);
            fclose(file);
            return WordTable{};
        }
    }

    free(line);
    fclose(file);

    return table;
}

uint64_t WordTable::fingerprint() const
{
    uint64_t hash = UINT64_C(1469598103934665603);

    for (size_t word_index = 0; word_index < count; ++word_index) {
        const unsigned char *word = reinterpret_cast<const unsigned char *>(words[word_index]);

        for (size_t char_index = 0; word[char_index] != '\0'; ++char_index) {
            hash ^= word[char_index];
            hash *= UINT64_C(1099511628211);
        }

        hash ^= 0;
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

void WordTable::print() const
{
    for (size_t word_index = 0; word_index < count; ++word_index) {
        printf("%s\n", words[word_index]);
    }
}

void WordTable::destroy()
{
    word_set.clear();
    free_words(words, count);
    words = NULL;
    count = 0;
}
