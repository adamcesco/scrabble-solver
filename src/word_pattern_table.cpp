#include "word_pattern_table.h"

#include "utils.h"

#include <algorithm>
#include <limits>
#include <stdio.h>
#include <string.h>

struct PatternRecord {
    PatternRow fixed_tiles;
    PatternBytes rack_letters;
    uint32_t word_index;
};

static int init_word_pattern_buckets(WordPatternTable *table)
{
    try {
        table->buckets_by_length.resize(BOARD_SIZE + 1);
        for (uint8_t length = 1; length <= BOARD_SIZE; ++length) {
            table->buckets_by_length[length].resize((size_t)UINT16_C(1) << length);
        }
    } catch (...) {
        return 0;
    }

    return 1;
}

static size_t pattern_record_count_for_word(const char *word)
{
    size_t word_length = strlen(word);
    size_t count = 0;

    if (word_length == 0 || word_length > BOARD_SIZE) {
        return 0;
    }

    for (uint16_t pattern_mask = 1; pattern_mask < (uint16_t)(UINT16_C(1) << word_length); ++pattern_mask) {
        if (__builtin_popcount(pattern_mask) <= 7) {
            count++;
        }
    }

    return count;
}

static int reserve_pattern_records_for_words(std::vector<PatternRecord> *records, const WordTable &words)
{
    size_t record_count = 0;

    for (size_t word_index = 0; word_index < words.count; ++word_index) {
        size_t word_record_count = pattern_record_count_for_word(words.words[word_index]);

        if (record_count > SIZE_MAX - word_record_count) {
            return 0;
        }

        record_count += word_record_count;
    }

    if (record_count > (size_t)std::numeric_limits<uint32_t>::max()) {
        return 0;
    }

    try {
        records->reserve(record_count);
    } catch (...) {
        return 0;
    }

    return 1;
}

static PatternRow make_pattern_row_for_mask(PatternBytes packed_word, uint16_t occupied_mask, uint8_t word_length)
{
    PatternRow row = {};

    row.occupiedMask = occupied_mask;
    row.length = word_length;

    for (uint8_t index = 0; index < word_length; ++index) {
        if ((occupied_mask & bit_at_u16(index)) == 0) {
            continue;
        }

        row.tiles |= put_byte_128(get_byte_128(packed_word, index, BOARD_SIZE), index, BOARD_SIZE);
    }

    return row;
}

static PatternBytes sorted_anagram_key_for_mask(PatternBytes packed_word, uint16_t pattern_mask, uint8_t word_length)
{
    uint8_t letters[7];
    uint8_t letter_count = 0;
    PatternBytes key = 0;

    for (uint8_t index = 0; index < word_length; ++index) {
        if ((pattern_mask & bit_at_u16(index)) == 0) {
            continue;
        }

        uint8_t letter = get_byte_128(packed_word, index, BOARD_SIZE);
        uint8_t insert_at = letter_count;

        while (insert_at > 0 && letters[insert_at - 1] > letter) {
            letters[insert_at] = letters[insert_at - 1];
            insert_at--;
        }

        letters[insert_at] = letter;
        letter_count++;
    }

    for (uint8_t index = 0; index < letter_count; ++index) {
        key |= put_byte_128(letters[index], index, BOARD_SIZE);
    }

    return key;
}

static int append_pattern_records_for_word(std::vector<PatternRecord> *records, const char *word, uint32_t word_index)
{
    uint8_t word_length = strlen(word);
    PatternBytes packed_word = 0;

    if (word_length == 0 || word_length > BOARD_SIZE) {
        return 1;
    }

    uint16_t pattern_count = (uint16_t)(UINT16_C(1) << word_length);
    uint16_t active_bits = low_bit_mask_u16(word_length);
    memcpy(&packed_word, word, word_length + 1);

    for (uint16_t pattern_mask = 0; pattern_mask < pattern_count; ++pattern_mask) {
        uint8_t anagram_length = __builtin_popcount(pattern_mask);

        if (anagram_length > 7 || anagram_length == 0) {
            continue;
        }

        uint16_t fixed_mask = (uint16_t)(active_bits & ~pattern_mask);
        PatternRow pattern_row = make_pattern_row_for_mask(packed_word, fixed_mask, word_length);
        PatternBytes packed_anagram = sorted_anagram_key_for_mask(packed_word, pattern_mask, word_length);

        try {
            records->push_back(PatternRecord{
                pattern_row,
                packed_anagram,
                word_index
            });
        } catch (...) {
            return 0;
        }
    }

    return 1;
}

static int build_pattern_records_from_words(std::vector<PatternRecord> *records, const WordTable &words)
{
    if (!reserve_pattern_records_for_words(records, words)) {
        return 0;
    }

    for (size_t word_index = 0; word_index < words.count; ++word_index) {
        if (!append_pattern_records_for_word(records, words.words[word_index], (uint32_t)word_index)) {
            return 0;
        }
    }

    return 1;
}

static int pattern_record_less(const PatternRecord &left, const PatternRecord &right)
{
    if (left.fixed_tiles.length != right.fixed_tiles.length) {
        return left.fixed_tiles.length < right.fixed_tiles.length;
    }

    if (left.fixed_tiles.occupiedMask != right.fixed_tiles.occupiedMask) {
        return left.fixed_tiles.occupiedMask < right.fixed_tiles.occupiedMask;
    }

    if (left.fixed_tiles.tiles != right.fixed_tiles.tiles) {
        return left.fixed_tiles.tiles < right.fixed_tiles.tiles;
    }

    if (left.rack_letters != right.rack_letters) {
        return left.rack_letters < right.rack_letters;
    }

    return left.word_index < right.word_index;
}

static int pattern_record_matches_pattern(const PatternRecord &record, const PatternRow &pattern)
{
    return
        record.fixed_tiles.length == pattern.length &&
        record.fixed_tiles.occupiedMask == pattern.occupiedMask &&
        record.fixed_tiles.tiles == pattern.tiles;
}

static int pack_sorted_pattern_records(WordPatternTable *table, const std::vector<PatternRecord> &records)
{
    try {
        table->word_indices.reserve(records.size());
    } catch (...) {
        return 0;
    }

    for (size_t record_index = 0; record_index < records.size();) {
        PatternRow pattern = records[record_index].fixed_tiles;
        WordPatternBucket &bucket = table->buckets_by_length[pattern.length][pattern.occupiedMask];
        WordPattern pattern_entry = {
            pattern.tiles,
            {
                (uint32_t)table->anagram_groups.size(),
                0
            }
        };

        while (record_index < records.size() && pattern_record_matches_pattern(records[record_index], pattern)) {
            PatternBytes anagram = records[record_index].rack_letters;
            WordPatternAnagramGroup anagram_entry = {
                anagram,
                {
                    (uint32_t)table->word_indices.size(),
                    0
                }
            };

            while (
                record_index < records.size() &&
                pattern_record_matches_pattern(records[record_index], pattern) &&
                records[record_index].rack_letters == anagram
            ) {
                table->word_indices.push_back(records[record_index].word_index);
                anagram_entry.word_indices.count++;
                record_index++;
            }

            table->anagram_groups.push_back(anagram_entry);
            pattern_entry.anagrams.count++;
        }

        bucket.push_back(pattern_entry);
    }

    return 1;
}

WordPatternTable WordPatternTable::from_words(const WordTable &words)
{
    WordPatternTable table = {};
    std::vector<PatternRecord> records;

    if (!init_word_pattern_buckets(&table)) {
        return {};
    }

    if (words.count == 0) {
        return table;
    }

    if (words.count > (size_t)std::numeric_limits<uint32_t>::max()) {
        return {};
    }

    if (!build_pattern_records_from_words(&records, words)) {
        return {};
    }

    std::sort(records.begin(), records.end(), pattern_record_less);

    if (!pack_sorted_pattern_records(&table, records)) {
        return {};
    }

    table.dictionary_fingerprint = words.fingerprint();
    table.dictionary_word_count = words.count;

    return table;
}

int WordPatternTable::is_empty() const
{
    for (const WordPatternBucketsByMask &length_buckets : buckets_by_length) {
        for (const WordPatternBucket &bucket : length_buckets) {
            if (!bucket.empty()) {
                return 0;
            }
        }
    }

    return 1;
}

size_t WordPatternTable::largest_binary_search_domain() const
{
    size_t largest = 0;

    for (const WordPatternBucketsByMask &length_buckets : buckets_by_length) {
        for (const WordPatternBucket &bucket : length_buckets) {
            if (bucket.size() > largest) {
                largest = bucket.size();
            }
        }
    }

    return largest;
}

int WordPatternTable::anagram_contains_word(const WordTable &words, const WordPatternAnagramGroup &entry, const char *word) const
{
    for (size_t offset = 0; offset < entry.word_indices.count; ++offset) {
        if (strcmp(word_at(words, entry, offset), word) == 0) {
            return 1;
        }
    }

    return 0;
}

static int write_pattern_bytes(FILE *file, PatternBytes value)
{
    uint64_t low = (uint64_t)value;
    uint64_t high = (uint64_t)(value >> 64);

    return write_u64(file, low) && write_u64(file, high);
}

static int read_pattern_bytes(FILE *file, PatternBytes *value)
{
    uint64_t low;
    uint64_t high;

    if (!read_u64(file, &low) || !read_u64(file, &high)) {
        return 0;
    }

    *value = (((PatternBytes)high) << 64) | (PatternBytes)low;
    return 1;
}

static int count_serialized_pattern_buckets(const WordPatternTable *table, uint64_t *bucket_count)
{
    *bucket_count = 0;

    for (const WordPatternBucketsByMask &length_buckets : table->buckets_by_length) {
        for (const WordPatternBucket &bucket : length_buckets) {
            if (bucket.empty()) {
                continue;
            }

            if (bucket.size() > (size_t)std::numeric_limits<uint32_t>::max()) {
                return 0;
            }

            (*bucket_count)++;
        }
    }

    return 1;
}

static int write_pattern_bucket(FILE *file, size_t length, size_t fixed_mask, const WordPatternBucket &bucket)
{
    int ok =
        write_u32(file, (uint32_t)length) &&
        write_u32(file, (uint32_t)fixed_mask) &&
        write_u64(file, (uint64_t)bucket.size());

    for (size_t index = 0; ok && index < bucket.size(); ++index) {
        const WordPattern &entry = bucket[index];

        ok =
            write_pattern_bytes(file, entry.fixed_tiles) &&
            write_u32(file, entry.anagrams.start) &&
            write_u32(file, entry.anagrams.count);
    }

    return ok;
}

static int read_pattern_bucket(FILE *file, WordPatternTable *table)
{
    uint32_t length;
    uint32_t fixed_mask;
    uint64_t pattern_count;

    int ok =
        read_u32(file, &length) &&
        read_u32(file, &fixed_mask) &&
        read_u64(file, &pattern_count) &&
        length <= BOARD_SIZE &&
        fixed_mask < (UINT32_C(1) << length) &&
        pattern_count <= (uint64_t)std::numeric_limits<uint32_t>::max();

    if (!ok) {
        return 0;
    }

    WordPatternBucket &bucket = table->buckets_by_length[length][fixed_mask];
    try {
        bucket.resize((size_t)pattern_count);
    } catch (...) {
        return 0;
    }

    for (size_t index = 0; index < bucket.size(); ++index) {
        WordPattern &entry = bucket[index];

        ok =
            read_pattern_bytes(file, &entry.fixed_tiles) &&
            read_u32(file, &entry.anagrams.start) &&
            read_u32(file, &entry.anagrams.count) &&
            (size_t)entry.anagrams.start + entry.anagrams.count <= table->anagram_groups.size();

        if (!ok) {
            return 0;
        }
    }

    return 1;
}

int WordPatternTable::save(const WordTable &words, const char *cache_path) const
{
    static const char magic[8] = {'W', 'P', 'T', 'I', 'D', 'X', '1', '\0'};
    uint64_t bucket_count = 0;

    if (anagram_groups.size() > (size_t)std::numeric_limits<uint32_t>::max() ||
        word_indices.size() > (size_t)std::numeric_limits<uint32_t>::max()) {
        return 0;
    }

    if (!count_serialized_pattern_buckets(this, &bucket_count)) {
        return 0;
    }

    FILE *file = fopen(cache_path, "wb");
    if (file == NULL) {
        return 0;
    }

    int ok =
        write_exact(file, magic, sizeof(magic)) &&
        write_u32(file, 3) &&
        write_u32(file, BOARD_SIZE) &&
        write_u32(file, UINT32_C(0x01020304)) &&
        write_u64(file, (uint64_t)words.count) &&
        write_u64(file, words.fingerprint()) &&
        write_u64(file, bucket_count) &&
        write_u64(file, (uint64_t)anagram_groups.size()) &&
        write_u64(file, (uint64_t)word_indices.size());

    for (size_t length = 0; ok && length < buckets_by_length.size(); ++length) {
        const WordPatternBucketsByMask &length_buckets = buckets_by_length[length];

        for (size_t fixed_mask = 0; ok && fixed_mask < length_buckets.size(); ++fixed_mask) {
            const WordPatternBucket &bucket = length_buckets[fixed_mask];

            if (bucket.empty()) {
                continue;
            }

            ok = write_pattern_bucket(file, length, fixed_mask, bucket);
        }
    }

    for (size_t index = 0; ok && index < anagram_groups.size(); ++index) {
        const WordPatternAnagramGroup &entry = anagram_groups[index];
        ok =
            write_pattern_bytes(file, entry.rack_letters) &&
            write_u32(file, entry.word_indices.start) &&
            write_u32(file, entry.word_indices.count);
    }

    for (size_t index = 0; ok && index < word_indices.size(); ++index) {
        ok = write_u32(file, word_indices[index]);
    }

    if (fclose(file) != 0) {
        return 0;
    }

    return ok;
}

WordPatternTable WordPatternTable::load(const WordTable &words, const char *cache_path)
{
    static const char expected_magic[8] = {'W', 'P', 'T', 'I', 'D', 'X', '1', '\0'};
    WordPatternTable table = {};
    char magic[8];
    uint32_t version;
    uint32_t board_size;
    uint32_t endian_marker;
    uint64_t word_count;
    uint64_t fingerprint;
    uint64_t bucket_count;
    uint64_t anagram_count;
    uint64_t word_index_count;

    FILE *file = fopen(cache_path, "rb");
    if (file == NULL) {
        return table;
    }

    int ok =
        read_exact(file, magic, sizeof(magic)) &&
        read_u32(file, &version) &&
        read_u32(file, &board_size) &&
        read_u32(file, &endian_marker) &&
        read_u64(file, &word_count) &&
        read_u64(file, &fingerprint) &&
        read_u64(file, &bucket_count) &&
        read_u64(file, &anagram_count) &&
        read_u64(file, &word_index_count);

    ok = ok &&
        memcmp(magic, expected_magic, sizeof(magic)) == 0 &&
        version == 3 &&
        board_size == BOARD_SIZE &&
        endian_marker == UINT32_C(0x01020304) &&
        word_count == (uint64_t)words.count &&
        fingerprint == words.fingerprint() &&
        bucket_count <= (uint64_t)std::numeric_limits<uint32_t>::max() &&
        anagram_count <= (uint64_t)std::numeric_limits<uint32_t>::max() &&
        word_index_count <= (uint64_t)std::numeric_limits<uint32_t>::max();

    if (ok) {
        try {
            table.anagram_groups.resize((size_t)anagram_count);
            table.word_indices.resize((size_t)word_index_count);
        } catch (...) {
            ok = 0;
        }
    }

    if (ok) {
        ok = init_word_pattern_buckets(&table);
    }

    for (size_t bucket_index = 0; ok && bucket_index < (size_t)bucket_count; ++bucket_index) {
        ok = read_pattern_bucket(file, &table);
    }

    for (size_t index = 0; ok && index < table.anagram_groups.size(); ++index) {
        WordPatternAnagramGroup &entry = table.anagram_groups[index];
        ok =
            read_pattern_bytes(file, &entry.rack_letters) &&
            read_u32(file, &entry.word_indices.start) &&
            read_u32(file, &entry.word_indices.count) &&
            (size_t)entry.word_indices.start + entry.word_indices.count <= table.word_indices.size();
    }

    for (size_t index = 0; ok && index < table.word_indices.size(); ++index) {
        ok = read_u32(file, &table.word_indices[index]) && table.word_indices[index] < words.count;
    }

    fclose(file);

    if (!ok) {
        return WordPatternTable{};
    }

    table.dictionary_fingerprint = fingerprint;
    table.dictionary_word_count = (size_t)word_count;
    return table;
}

void WordPatternTable::print(const WordTable &words) const
{
    for (const WordPatternBucketsByMask &length_buckets : buckets_by_length) {
        for (const WordPatternBucket &bucket : length_buckets) {
            for (const WordPattern &pattern_entry : bucket) {
                const char *pattern = reinterpret_cast<const char *>(&pattern_entry.fixed_tiles);
                char printable_pattern[BOARD_SIZE + 1];

                for (uint8_t index = 0; index < BOARD_SIZE; ++index) {
                    printable_pattern[index] = pattern[index] == '\0' ? ' ' : pattern[index];
                }
                printable_pattern[BOARD_SIZE] = '\0';

                printf("pattern \"%.*s\"\n", BOARD_SIZE, printable_pattern);

                for (uint32_t anagram_offset = 0; anagram_offset < pattern_entry.anagrams.count; ++anagram_offset) {
                    const WordPatternAnagramGroup &anagram_entry = anagram_groups[(size_t)pattern_entry.anagrams.start + anagram_offset];
                    const char *letters = reinterpret_cast<const char *>(&anagram_entry.rack_letters);

                    printf("  letters \"%.*s\":", 7, letters);

                    for (uint32_t word_offset = 0; word_offset < anagram_entry.word_indices.count; ++word_offset) {
                        printf(" %s", word_at(words, anagram_entry, word_offset));
                    }

                    putchar('\n');
                }
            }
        }
    }
}
