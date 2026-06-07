#ifndef DICTIONARY_H
#define DICTIONARY_H

#include "board.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char **words;
    size_t count;
    void *hash_state;
} WordTable; // hash-table that holds all words within the dictionary

typedef struct {
    const char *word;
    Row rows[BOARD_SIZE];
    uint16_t valid_starts;
} WordStartRowEntry;

typedef struct {
    WordStartRowEntry *entries;
    size_t count;
    void *hash_state;
} WordStartRowTable; // hash-map that when given a word and starting location, it returns a Row that has that word at that starting location

WordTable words_from_file(const char *file_path);
int word_table_contains(const WordTable *table, const char *word);
void word_table_print(const WordTable *table);
void word_table_destroy(WordTable *table);

WordStartRowTable word_start_row_table_from_word_table(const WordTable *words);
const Row *word_start_row_table_get(const WordStartRowTable *table, const char *word, size_t start);
void word_start_row_table_print(const WordStartRowTable *table);
void word_start_row_table_destroy(WordStartRowTable *table);

#endif
