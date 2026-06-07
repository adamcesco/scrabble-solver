#define _GNU_SOURCE

#include "dictionary.h"

#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TILE_BITS 5

static void trim_line_ending(char *line)
{
    line[strcspn(line, "\r\n")] = '\0';
}

static void print_bits(RowTiles value, int bit_count)
{
    for (int bit_index = bit_count - 1; bit_index >= 0; --bit_index) {
        putchar((value & ((RowTiles)1 << bit_index)) ? '1' : '0');

        if (bit_index > 0 && bit_index % TILE_BITS == 0) {
            putchar(' ');
        }
    }
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

static Row make_word_start_row_at(const char *word, size_t start)
{
    char tiles[BOARD_SIZE + 1];
    size_t word_length = strlen(word);

    memset(tiles, '.', BOARD_SIZE);
    memcpy(&tiles[start], word, word_length);
    tiles[BOARD_SIZE] = '\0';

    return make_row(tiles);
}

static int add_unique_word(WordTable *table, const char *line)
{
    ENTRY item;
    ENTRY *found = NULL;
    struct hsearch_data *hash_state = table->hash_state;
    char *word;

    item.key = (char *)line;
    item.data = NULL;
    if (hsearch_r(item, FIND, &found, hash_state) && found != NULL) {
        return 1;
    }

    word = copy_word(line);
    if (word == NULL) {
        return 0;
    }

    item.key = word;
    item.data = word;
    found = NULL;
    if (!hsearch_r(item, ENTER, &found, hash_state) || found == NULL) {
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

    table.hash_state = calloc(1, sizeof(struct hsearch_data));
    if (table.hash_state == NULL) {
        free(table.words);
        fclose(file);
        return (WordTable){0};
    }

    if (!hcreate_r(max_word_count + (max_word_count / 4) + 1, table.hash_state)) {
        free(table.hash_state);
        free(table.words);
        fclose(file);
        return (WordTable){0};
    }

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
    ENTRY *found = NULL;

    if (table == NULL || table->words == NULL || table->hash_state == NULL || word == NULL) {
        return 0;
    }

    item.key = (char *)word;
    item.data = NULL;

    return hsearch_r(item, FIND, &found, table->hash_state) && found != NULL;
}

void word_table_print(const WordTable *table)
{
    if (table == NULL || table->words == NULL) {
        return;
    }

    for (size_t word_index = 0; word_index < table->count; ++word_index) {
        printf("%s\n", table->words[word_index]);
    }
}

WordStartRowTable word_start_row_table_from_word_table(const WordTable *words)
{
    WordStartRowTable table = {0};

    if (words == NULL || words->words == NULL || words->count == 0) {
        return table;
    }

    table.entries = calloc(words->count, sizeof(table.entries[0]));
    if (table.entries == NULL) {
        return table;
    }

    table.hash_state = calloc(1, sizeof(struct hsearch_data));
    if (table.hash_state == NULL) {
        free(table.entries);
        return (WordStartRowTable){0};
    }

    if (!hcreate_r(words->count + (words->count / 4) + 1, table.hash_state)) {
        free(table.hash_state);
        free(table.entries);
        return (WordStartRowTable){0};
    }

    for (size_t word_index = 0; word_index < words->count; ++word_index) {
        ENTRY item;
        ENTRY *found = NULL;
        WordStartRowEntry *entry = &table.entries[word_index];
        size_t word_length = strlen(words->words[word_index]);

        entry->word = words->words[word_index];

        for (size_t start = 0; start < BOARD_SIZE; ++start) {
            if (word_length <= BOARD_SIZE - start) {
                entry->rows[start] = make_word_start_row_at(entry->word, start);
                entry->valid_starts |= (uint16_t)(UINT16_C(1) << start);
            }
        }

        item.key = (char *)entry->word;
        item.data = entry;
        found = NULL;
        if (!hsearch_r(item, ENTER, &found, table.hash_state) || found == NULL) {
            word_start_row_table_destroy(&table);
            return (WordStartRowTable){0};
        }

        table.count++;
    }

    return table;
}

const Row *word_start_row_table_get(const WordStartRowTable *table, const char *word, size_t start)
{
    ENTRY item;
    ENTRY *found = NULL;
    WordStartRowEntry *entry;

    if (table == NULL || table->entries == NULL || table->hash_state == NULL || word == NULL || start >= BOARD_SIZE) {
        return NULL;
    }

    item.key = (char *)word;
    item.data = NULL;

    if (!hsearch_r(item, FIND, &found, table->hash_state) || found == NULL) {
        return NULL;
    }

    entry = found->data;
    if ((entry->valid_starts & (uint16_t)(UINT16_C(1) << start)) == 0) {
        return NULL;
    }

    return &entry->rows[start];
}

void word_start_row_table_print(const WordStartRowTable *table)
{
    if (table == NULL || table->entries == NULL) {
        return;
    }

    for (size_t entry_index = 0; entry_index < table->count; ++entry_index) {
        const WordStartRowEntry *entry = &table->entries[entry_index];

        for (size_t start = 0; start < BOARD_SIZE; ++start) {
            const Row *row;

            if ((entry->valid_starts & (uint16_t)(UINT16_C(1) << start)) == 0) {
                continue;
            }

            row = &entry->rows[start];
            printf("%s start %2zu ", entry->word, start);
            print_bits(row->tiles, BOARD_SIZE * TILE_BITS);
            putchar('\n');
        }
    }
}

void word_table_destroy(WordTable *table)
{
    if (table == NULL) {
        return;
    }

    if (table->hash_state != NULL) {
        hdestroy_r(table->hash_state);
        free(table->hash_state);
    }

    free_words(table->words, table->count);
    table->words = NULL;
    table->count = 0;
    table->hash_state = NULL;
}

void word_start_row_table_destroy(WordStartRowTable *table)
{
    if (table == NULL) {
        return;
    }

    if (table->hash_state != NULL) {
        hdestroy_r(table->hash_state);
        free(table->hash_state);
    }

    free(table->entries);
    table->entries = NULL;
    table->count = 0;
    table->hash_state = NULL;
}
