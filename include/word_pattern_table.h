#ifndef WORD_PATTERN_TABLE_H
#define WORD_PATTERN_TABLE_H

#include "board.h"
#include "word_table.h"

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include <vector>

typedef unsigned __int128 PatternBytes;

struct PatternRow {
    PatternBytes tiles = 0;
    uint16_t occupiedMask = 0;
    uint8_t length = 0;
};

struct WordPatternRange {
    uint32_t start = 0;
    uint32_t count = 0;
};

struct WordPattern {
    PatternBytes fixed_tiles = 0;
    WordPatternRange anagrams;
};

struct WordPatternAnagramGroup {
    PatternBytes rack_letters = 0;
    WordPatternRange word_indices;
};

typedef std::vector<WordPattern> WordPatternBucket;
typedef std::vector<WordPatternBucket> WordPatternBucketsByMask;

struct WordPatternTable {
    std::vector<WordPatternBucketsByMask> buckets_by_length;
    std::vector<WordPatternAnagramGroup> anagram_groups;
    std::vector<uint32_t> word_indices;
    uint64_t dictionary_fingerprint = 0;
    size_t dictionary_word_count = 0;

    static WordPatternTable from_words(const WordTable &words);

    // Unsafe to call with a null cache_path.
    static WordPatternTable load(const WordTable &words, const char *cache_path);

    int is_empty() const;
    size_t largest_binary_search_domain() const;

    // Unsafe unless pattern.length is 1..BOARD_SIZE and occupiedMask fits that length.
    const WordPattern *get(const PatternRow &pattern) const;

    // Unsafe unless entry belongs to this table.
    const WordPatternAnagramGroup *get_anagram(const WordPattern &entry, PatternBytes anagram) const;

    // Unsafe unless entry belongs to this table, offset is in range, and words is the source dictionary.
    const char *word_at(const WordTable &words, const WordPatternAnagramGroup &entry, size_t offset) const;

    // Unsafe unless entry belongs to this table, words is the source dictionary, and word is non-null.
    int anagram_contains_word(const WordTable &words, const WordPatternAnagramGroup &entry, const char *word) const;

    // Unsafe to call with a null cache_path.
    int save(const WordTable &words, const char *cache_path) const;
    void print(const WordTable &words) const;
};

inline const WordPattern *WordPatternTable::get(const PatternRow &pattern) const
{
    const WordPatternBucket &bucket = buckets_by_length[pattern.length][pattern.occupiedMask];

    auto iter = std::lower_bound(
        bucket.begin(),
        bucket.end(),
        pattern.tiles,
        [](const WordPattern &entry, PatternBytes value) {
            return entry.fixed_tiles < value;
        }
    );

    if (iter == bucket.end() || iter->fixed_tiles != pattern.tiles) {
        return NULL;
    }

    return &*iter;
}

inline const WordPatternAnagramGroup *WordPatternTable::get_anagram(const WordPattern &entry, PatternBytes anagram) const
{
    auto begin = anagram_groups.begin() + entry.anagrams.start;
    auto end = begin + entry.anagrams.count;
    auto iter = std::lower_bound(
        begin,
        end,
        anagram,
        [](const WordPatternAnagramGroup &anagram_entry, PatternBytes value) {
            return anagram_entry.rack_letters < value;
        }
    );

    if (iter == end || iter->rack_letters != anagram) {
        return NULL;
    }

    return &*iter;
}

inline const char *WordPatternTable::word_at(const WordTable &words, const WordPatternAnagramGroup &entry, size_t offset) const
{
    uint32_t word_index = word_indices[(size_t)entry.word_indices.start + offset];
    return words.words[word_index];
}

#endif
