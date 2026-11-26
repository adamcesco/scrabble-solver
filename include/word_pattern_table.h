#ifndef WORD_PATTERN_TABLE_H
#define WORD_PATTERN_TABLE_H

#include "board.h"
#include "word_table.h"

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include <vector>

typedef unsigned __int128 PatternBytes;

struct WordPatternRange {
    uint32_t start = 0;
    uint32_t count = 0;
};

struct WordPattern {
    PatternBytes fixed_tiles = 0;
    WordPatternRange anagrams;
    uint32_t hash_index = 0; // One-based index into the table's anagram hashes.
};

struct WordPatternAnagramGroup {
    uint64_t rack_letters = 0;
    WordPatternRange word_indices;
};

static_assert(sizeof(WordPatternAnagramGroup) == 16);

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

    // Unsafe unless pattern.length is 1..BOARD_SIZE and occupiedMask fits that length.
    const WordPattern *get(const PatternRow &pattern) const;

    // Unsafe unless entry belongs to this table.
    const WordPatternAnagramGroup *get_anagram(const WordPattern &entry, uint64_t anagram) const;

    // Unsafe unless entry belongs to this table, offset is in range, and words is the source dictionary.
    const char *word_at(const WordTable &words, const WordPatternAnagramGroup &entry, size_t offset) const;

    // Unsafe to call with a null cache_path.
    int save(const WordTable &words, const char *cache_path) const;
    void print(const WordTable &words) const;

private:
    std::vector<std::vector<uint32_t>> anagram_hashes;
    bool build_anagram_hashes();

    static uint64_t fmix64(uint64_t key)
    {
        key ^= key >> 33;
        key *= UINT64_C(0xff51afd7ed558ccd);
        key ^= key >> 33;
        key *= UINT64_C(0xc4ceb9fe1a85ec53);
        return key ^ (key >> 33);
    }
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

inline const WordPatternAnagramGroup *WordPatternTable::get_anagram(const WordPattern &entry, uint64_t anagram) const
{
    if (entry.hash_index) {
        const auto &slots = anagram_hashes[entry.hash_index - 1];
        const size_t mask = slots.size() - 1;
        size_t slot = fmix64(anagram) & mask;
        while (uint32_t ordinal = slots[slot]) {
            const auto &group = anagram_groups[ordinal - 1];
            if (group.rack_letters == anagram) {
                return &group;
            }
            slot = (slot + 1) & mask;
        }
        return nullptr;
    }

    auto begin = anagram_groups.begin() + entry.anagrams.start;
    auto end = begin + entry.anagrams.count;
    auto iter = std::lower_bound(
        begin,
        end,
        anagram,
        [](const WordPatternAnagramGroup &anagram_entry, uint64_t value) {
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
