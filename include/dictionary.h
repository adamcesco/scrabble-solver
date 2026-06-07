#ifndef DICTIONARY_H
#define DICTIONARY_H

#include <stddef.h>

typedef struct {
    char **words;
    size_t count;
} WordTable;

WordTable words_from_file(const char *file_path);
int word_table_contains(const WordTable *table, const char *word);
void word_table_destroy(WordTable *table);

#endif
