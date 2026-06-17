#include "word_start_row_table.h"

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Row make_word_start_row_at(const char *word, size_t start)
{
    char tiles[BOARD_SIZE + 1];
    size_t word_length = strlen(word);

    memset(tiles, '.', BOARD_SIZE);
    memcpy(&tiles[start], word, word_length);
    tiles[BOARD_SIZE] = '\0';

    return make_row(tiles);
}

WordStartRowTable WordStartRowTable::from_words(const WordTable &words)
{
    WordStartRowTable table = {};

    if (words.count == 0) {
        return table;
    }

    table.entries = static_cast<WordStartRowEntry *>(calloc(words.count, sizeof(table.entries[0])));
    if (table.entries == NULL) {
        return table;
    }

    try {
        table.rows_by_word.reserve(words.count + (words.count / 4) + 1);
    } catch (...) {
        free(table.entries);
        return WordStartRowTable{};
    }

    for (size_t word_index = 0; word_index < words.count; ++word_index) {
        WordStartRowEntry *entry = &table.entries[word_index];
        size_t word_length = strlen(words.words[word_index]);

        entry->word = words.words[word_index];
        entry->word_length = (uint8_t)word_length;

        for (size_t start = 0; start < BOARD_SIZE; ++start) {
            if (word_length <= BOARD_SIZE - start) {
                entry->rows[start] = make_word_start_row_at(entry->word, start);
                entry->valid_starts |= (uint16_t)(UINT16_C(1) << start);
            }
        }

        try {
            table.rows_by_word.emplace(entry->word, entry);
        } catch (...) {
            table.destroy();
            return WordStartRowTable{};
        }

        table.count++;
    }

    return table;
}

const Row *WordStartRowTable::get(const char *word, size_t start) const
{
    if (start >= BOARD_SIZE) {
        return NULL;
    }

    auto found = rows_by_word.find(word);
    if (found == rows_by_word.end()) {
        return NULL;
    }

    WordStartRowEntry *entry = found->second;
    if ((entry->valid_starts & (uint16_t)(UINT16_C(1) << start)) == 0) {
        return NULL;
    }

    return &entry->rows[start];
}

void WordStartRowTable::print() const
{
    for (size_t entry_index = 0; entry_index < count; ++entry_index) {
        const WordStartRowEntry *entry = &entries[entry_index];

        for (size_t start = 0; start < BOARD_SIZE; ++start) {
            if ((entry->valid_starts & (uint16_t)(UINT16_C(1) << start)) == 0) {
                continue;
            }

            const Row *row = &entry->rows[start];
            printf("%s start %2zu ", entry->word, start);
            print_bits_grouped(stdout, row->tiles, BOARD_SIZE * ROW_TILE_BITS, ROW_TILE_BITS, 0);
            putchar('\n');
        }
    }
}

void WordStartRowTable::destroy()
{
    rows_by_word.clear();
    free(entries);
    entries = NULL;
    count = 0;
}
