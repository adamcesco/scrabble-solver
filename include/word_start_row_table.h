#ifndef WORD_START_ROW_TABLE_H
#define WORD_START_ROW_TABLE_H

#include "board.h"
#include "word_table.h"

#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include <unordered_map>

struct WordStartRowEntry {
    const char *word = NULL;
    uint8_t word_length = 0;
    Row rows[BOARD_SIZE] = {};
    uint16_t valid_starts = 0;
};

struct WordStartRowTable {
    WordStartRowEntry *entries = NULL;
    size_t count = 0;
    std::unordered_map<std::string_view, WordStartRowEntry *> rows_by_word;

    static WordStartRowTable from_words(const WordTable &words);

    // Unsafe after the source WordTable is destroyed; keys point into WordTable word storage.
    const Row *get(const char *word, size_t start) const;
    void print() const;
    void destroy();
};

#endif
