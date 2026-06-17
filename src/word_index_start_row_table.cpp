#include "word_index_start_row_table.h"

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

WordIndexStartRowTable WordIndexStartRowTable::from_words(const WordTable &words)
{
    WordIndexStartRowTable table = {};

    if (words.count == 0) {
        return table;
    }

    table.entries = static_cast<WordIndexStartRowEntry *>(calloc(words.count, sizeof(table.entries[0])));
    if (table.entries == NULL) {
        return table;
    }

    for (size_t word_index = 0; word_index < words.count; ++word_index) {
        WordIndexStartRowEntry *entry = &table.entries[word_index];
        size_t word_length = strlen(words.words[word_index]);

        entry->word_length = (uint8_t)word_length;

        for (size_t start = 0; start < BOARD_SIZE; ++start) {
            if (word_length <= BOARD_SIZE - start) {
                entry->rows[start] = make_word_start_row_at(words.words[word_index], start);
                entry->valid_starts |= (uint16_t)(UINT16_C(1) << start);
            }
        }

        table.count++;
    }

    return table;
}

void WordIndexStartRowTable::destroy()
{
    free(entries);
    entries = NULL;
    count = 0;
}
