#ifndef WORD_INDEX_START_ROW_TABLE_H
#define WORD_INDEX_START_ROW_TABLE_H

#include "board.h"
#include "word_table.h"

#include <stddef.h>
#include <stdint.h>

struct WordIndexStartRowEntry {
    uint8_t word_length = 0;
    Row rows[BOARD_SIZE] = {};
    uint16_t valid_starts = 0;
};

struct WordIndexStartRowTable {
    WordIndexStartRowEntry *entries = NULL;
    size_t count = 0;

    static WordIndexStartRowTable from_words(const WordTable &words);
    const Row *get(size_t word_index, size_t start) const;

    // Unsafe unless word_index and start are known valid for this table.
    const Row *get_unchecked(size_t word_index, size_t start) const;

    void destroy();
};

inline const Row *WordIndexStartRowTable::get(size_t word_index, size_t start) const
{
    if (word_index >= count || start >= BOARD_SIZE) {
        return NULL;
    }

    const WordIndexStartRowEntry *entry = &entries[word_index];
    if ((entry->valid_starts & (uint16_t)(UINT16_C(1) << start)) == 0) {
        return NULL;
    }

    return &entry->rows[start];
}

inline const Row *WordIndexStartRowTable::get_unchecked(size_t word_index, size_t start) const
{
    return &entries[word_index].rows[start];
}

#endif
