#include "word_pattern_table.h"

#include "utils.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdio.h>
#include <string.h>
#include <tuple>

struct PatternRecord {
    PatternBytes fixed_tiles;
    uint64_t rack_letters;
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

struct PackedWord {
    PatternBytes letters;
    uint32_t index;
};

static uint64_t sorted_rack_key(PatternBytes word, const uint8_t *slots, uint8_t count)
{
    uint8_t letters[7];
    for (uint8_t i = 0; i < count; ++i) {
        const uint8_t letter = get_byte_128(word, slots[i], BOARD_SIZE);
        uint8_t insert_at = i;
        while (insert_at > 0 && letters[insert_at - 1] > letter) {
            letters[insert_at] = letters[insert_at - 1];
            --insert_at;
        }
        letters[insert_at] = letter;
    }

    uint64_t key = 0;
    for (uint8_t i = 0; i < count; ++i) {
        key |= put_byte_64(letters[i], i);
    }
    return key;
}

static void build_pattern_bucket(WordPatternTable &table, const std::vector<PackedWord> &words,
                                 uint8_t length, uint16_t fixed_mask, std::vector<PatternRecord> &records)
{
    PatternBytes fixed_bytes = 0;
    uint8_t rack_slots[7];
    uint8_t rack_count = 0;
    for (uint8_t i = 0; i < length; ++i) {
        if (fixed_mask & bit_at_u16(i)) {
            fixed_bytes |= put_byte_128(0xff, i, BOARD_SIZE);
        } else {
            rack_slots[rack_count++] = i;
        }
    }

    records.clear();
    for (const PackedWord &word : words) {
        records.push_back({word.letters & fixed_bytes, sorted_rack_key(word.letters, rack_slots, rack_count), word.index});
    }
    std::sort(records.begin(), records.end(), [](const PatternRecord &a, const PatternRecord &b) {
        return std::tie(a.fixed_tiles, a.rack_letters, a.word_index) <
               std::tie(b.fixed_tiles, b.rack_letters, b.word_index);
    });

    auto &bucket = table.buckets_by_length[length][fixed_mask];
    for (const PatternRecord &record : records) {
        if (bucket.empty() || bucket.back().fixed_tiles != record.fixed_tiles) {
            bucket.push_back({record.fixed_tiles, {static_cast<uint32_t>(table.anagram_groups.size()), 0}});
        }
        auto &groups = bucket.back().anagrams;
        if (groups.count == 0 || table.anagram_groups.back().rack_letters != record.rack_letters) {
            table.anagram_groups.push_back({record.rack_letters, {static_cast<uint32_t>(table.word_indices.size()), 0}});
            ++groups.count;
        }
        ++table.anagram_groups.back().word_indices.count;
        table.word_indices.push_back(record.word_index);
    }
}

bool WordPatternTable::build_anagram_hashes()
{
    try {
        for (auto &length : buckets_by_length) {
            for (size_t mask = 0; mask < length.size(); ++mask) {
                if (__builtin_popcount(static_cast<unsigned>(mask)) > 1) {
                    continue;
                }
                for (auto &pattern : length[mask]) {
                    const auto range = pattern.anagrams;
                    if (range.count < 128) {
                        continue;
                    }
                    size_t capacity = 1;
                    while (capacity < 2 * size_t(range.count)) {
                        capacity *= 2;
                    }
                    auto &slots = anagram_hashes.emplace_back(capacity, 0);
                    pattern.hash_index = static_cast<uint32_t>(anagram_hashes.size());
                    for (uint32_t i = 0; i < range.count; ++i) {
                        const uint32_t ordinal = range.start + i;
                        size_t slot = fmix64(anagram_groups[ordinal].rack_letters) & (capacity - 1);
                        while (slots[slot]) {
                            slot = (slot + 1) & (capacity - 1);
                        }
                        slots[slot] = ordinal + 1;
                    }
                }
            }
        }
    } catch (...) {
        return false;
    }
    return true;
}

WordPatternTable WordPatternTable::from_words(const WordTable &words)
{
    WordPatternTable table;
    if (words.count > UINT32_MAX || !init_word_pattern_buckets(&table)) {
        return {};
    }
    if (words.count == 0) {
        return table;
    }

    try {
        std::array<std::vector<PackedWord>, BOARD_SIZE + 1> by_length;
        for (size_t i = 0; i < words.count; ++i) {
            const size_t length = strlen(words.words[i]);
            if (length == 0 || length > BOARD_SIZE) {
                continue;
            }
            PatternBytes packed = 0;
            for (size_t slot = 0; slot < length; ++slot) {
                packed |= put_byte_128(words.words[i][slot], slot, BOARD_SIZE);
            }
            by_length[length].push_back({packed, static_cast<uint32_t>(i)});
        }

        std::array<std::vector<uint16_t>, BOARD_SIZE + 1> fixed_masks;
        uint64_t record_count = 0;
        for (uint8_t length = 1; length <= BOARD_SIZE; ++length) {
            if (by_length[length].empty()) {
                continue;
            }
            const uint16_t all_slots = low_bit_mask_u16(length);
            for (uint16_t mask = 0; mask < all_slots; ++mask) {
                if (__builtin_popcount(all_slots ^ mask) <= 7) {
                    fixed_masks[length].push_back(mask);
                }
            }
            record_count += uint64_t(fixed_masks[length].size()) * by_length[length].size();
        }
        if (record_count > UINT32_MAX) {
            return {};
        }
        table.word_indices.reserve(record_count);
        table.anagram_groups.reserve(record_count);

        // Group ranges follow v3's bucket order. Sorting one bucket at a time
        // bounds temporary storage by the number of dictionary words at one length.
        std::vector<PatternRecord> records;
        for (uint8_t length = 1; length <= BOARD_SIZE; ++length) {
            records.reserve(by_length[length].size());
            for (uint16_t mask : fixed_masks[length]) {
                build_pattern_bucket(table, by_length[length], length, mask, records);
            }
        }
    } catch (...) {
        return {};
    }

    if (!table.build_anagram_hashes()) {
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

// v3 uses board-width keys. Slot zero has a different bit offset in a rack key
// on big-endian hosts, while successive slots have the same spacing.
static unsigned v3_rack_shift()
{
    return byte_shift_128(0, BOARD_SIZE) - byte_shift_64(0);
}

static PatternBytes v3_rack_key(uint64_t rack_letters)
{
    return static_cast<PatternBytes>(rack_letters) << v3_rack_shift();
}

static bool read_v3_rack_key(FILE *file, uint64_t &rack_letters)
{
    PatternBytes wide;
    if (!read_pattern_bytes(file, &wide)) {
        return false;
    }
    rack_letters = static_cast<uint64_t>(wide >> v3_rack_shift());
    return get_byte_64(rack_letters, 7) == 0 && v3_rack_key(rack_letters) == wide;
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
            write_pattern_bytes(file, v3_rack_key(entry.rack_letters)) &&
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

struct PatternCacheHeader {
    uint64_t fingerprint;
    uint64_t bucket_count;
    uint64_t anagram_count;
    uint64_t word_index_count;
};

static bool read_matching_cache_header(FILE *file, const WordTable &words, PatternCacheHeader &header)
{
    static const char expected_magic[8] = {'W', 'P', 'T', 'I', 'D', 'X', '1', '\0'};
    char magic[8];
    uint32_t version;
    uint32_t board_size;
    uint32_t endian_marker;
    uint64_t word_count;
    return
        read_exact(file, magic, sizeof(magic)) &&
        read_u32(file, &version) &&
        read_u32(file, &board_size) &&
        read_u32(file, &endian_marker) &&
        read_u64(file, &word_count) &&
        read_u64(file, &header.fingerprint) &&
        read_u64(file, &header.bucket_count) &&
        read_u64(file, &header.anagram_count) &&
        read_u64(file, &header.word_index_count) &&
        memcmp(magic, expected_magic, sizeof(magic)) == 0 &&
        version == 3 &&
        board_size == BOARD_SIZE &&
        endian_marker == UINT32_C(0x01020304) &&
        word_count == (uint64_t)words.count &&
        header.fingerprint == words.fingerprint() &&
        header.bucket_count <= (uint64_t)std::numeric_limits<uint32_t>::max() &&
        header.anagram_count <= (uint64_t)std::numeric_limits<uint32_t>::max() &&
        header.word_index_count <= (uint64_t)std::numeric_limits<uint32_t>::max();
}

WordPatternTable WordPatternTable::load(const WordTable &words, const char *cache_path)
{
    WordPatternTable table = {};
    FILE *file = fopen(cache_path, "rb");
    if (file == NULL) {
        return table;
    }
    PatternCacheHeader header;
    int ok = read_matching_cache_header(file, words, header);
    if (ok) {
        try {
            table.anagram_groups.resize((size_t)header.anagram_count);
            table.word_indices.resize((size_t)header.word_index_count);
        } catch (...) {
            ok = 0;
        }
    }

    if (ok) {
        ok = init_word_pattern_buckets(&table);
    }

    for (size_t bucket_index = 0; ok && bucket_index < (size_t)header.bucket_count; ++bucket_index) {
        ok = read_pattern_bucket(file, &table);
    }

    for (size_t index = 0; ok && index < table.anagram_groups.size(); ++index) {
        WordPatternAnagramGroup &entry = table.anagram_groups[index];
        ok =
            read_v3_rack_key(file, entry.rack_letters) &&
            read_u32(file, &entry.word_indices.start) &&
            read_u32(file, &entry.word_indices.count) &&
            (size_t)entry.word_indices.start + entry.word_indices.count <= table.word_indices.size();
    }

    for (size_t index = 0; ok && index < table.word_indices.size(); ++index) {
        ok = read_u32(file, &table.word_indices[index]) && table.word_indices[index] < words.count;
    }

    fclose(file);

    if (!ok || !table.build_anagram_hashes()) {
        return WordPatternTable{};
    }

    table.dictionary_fingerprint = header.fingerprint;
    table.dictionary_word_count = words.count;
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
