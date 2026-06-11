#ifndef DICTIONARY_H
#define DICTIONARY_H

#include "board.h"

#include <stddef.h>
#include <stdint.h>
#include <vector>

typedef unsigned __int128 PatternBytes;

typedef struct {
    PatternBytes tiles;
    PatternBytes keepMask;
    uint16_t occupiedMask;
    uint8_t length;
} PatternRow;

typedef struct {
    char **words;
    size_t count;
    void *hash_state;
} WordTable; // hash-table that holds all words within the dictionary

typedef struct {
    const char *word;
    uint8_t word_length;
    Row rows[BOARD_SIZE];
    uint16_t valid_starts;
} WordStartRowEntry;

typedef struct {
    WordStartRowEntry *entries;
    size_t count;
    void *hash_state;
} WordStartRowTable; // hash-map that when given a word and starting location, it returns a Row that has that word at that starting location

typedef struct {
    uint32_t start;
    uint32_t count;
} WordPatternRange;

typedef struct {
    PatternBytes fixed_tiles;
    WordPatternRange anagrams;
} WordPattern;

typedef struct {
    PatternBytes rack_letters;
    WordPatternRange word_indices;
} WordPatternAnagramGroup;

typedef std::vector<WordPattern> WordPatternBucket;
typedef std::vector<WordPatternBucket> WordPatternBucketsByMask;

typedef struct {
    std::vector<WordPatternBucketsByMask> buckets_by_length;
    std::vector<WordPatternAnagramGroup> anagram_groups;
    std::vector<uint32_t> word_indices;
    uint64_t dictionary_fingerprint;
    size_t dictionary_word_count;
} WordPatternTable;

WordTable words_from_file(const char *file_path);
int word_table_contains(const WordTable *table, const char *word);
void word_table_print(const WordTable *table);
void word_table_destroy(WordTable *table);

WordStartRowTable word_start_row_table_from_word_table(const WordTable *words);
const Row *word_start_row_table_get(const WordStartRowTable *table, const char *word, size_t start);
void word_start_row_table_print(const WordStartRowTable *table);
void word_start_row_table_destroy(WordStartRowTable *table);

WordPatternTable word_pattern_table_from_word_table(const WordTable *words);
int word_pattern_table_is_empty(const WordPatternTable *table);
size_t word_pattern_table_largest_binary_search_domain(const WordPatternTable *table);
const WordPattern *word_pattern_table_get(const WordPatternTable *table, const PatternRow *pattern);
const WordPatternAnagramGroup *word_pattern_entry_get_anagram(const WordPatternTable *table, const WordPattern *entry, PatternBytes anagram);
const char *word_pattern_table_word_at(const WordPatternTable *table, const WordTable *words, const WordPatternAnagramGroup *entry, size_t offset);
int word_pattern_anagram_contains_word(const WordPatternTable *table, const WordTable *words, const WordPatternAnagramGroup *entry, const char *word);
int word_pattern_table_save(const WordPatternTable *table, const WordTable *words, const char *cache_path);
WordPatternTable word_pattern_table_load(const WordTable *words, const char *cache_path);
void word_pattern_table_print(const WordPatternTable *table, const WordTable *words);

#endif
