#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "dictionary.h"
#include "utils.h"

#include <ctype.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

typedef std::unordered_set<std::string> WordSetHash;
typedef std::unordered_map<std::string, WordStartRowEntry *> WordStartRowHash;

/* File parsing and general word helpers */

static void trim_line_ending(char *line)
{
    line[strcspn(line, "\r\n")] = '\0';
}

static void uppercase_alpha_chars(char *word)
{
    for (size_t i = 0; word[i] != '\0'; ++i) {
        unsigned char ch = (unsigned char)word[i];

        if (isalpha(ch)) {
            word[i] = (char)toupper(ch);
        }
    }
}

static char *copy_word(const char *word)
{
    size_t length = strlen(word) + 1;
    char *copy = static_cast<char *>(malloc(length));

    if (copy != NULL) {
        memcpy(copy, word, length);
    }

    return copy;
}

static size_t count_words(FILE *file)
{
    char *line = NULL;
    size_t line_capacity = 0;
    size_t word_count = 0;

    while (getline(&line, &line_capacity, file) != -1) {
        trim_line_ending(line);

        if (line[0] != '\0') {
            word_count++;
        }
    }

    free(line);
    rewind(file);

    return word_count;
}

static void free_words(char **words, size_t count)
{
    for (size_t word_index = 0; word_index < count; ++word_index) {
        free(words[word_index]);
    }

    free(words);
}

static Row make_word_start_row_at(const char *word, size_t start)
{
    char tiles[BOARD_SIZE + 1];
    size_t word_length = strlen(word);

    memset(tiles, '.', BOARD_SIZE);
    memcpy(&tiles[start], word, word_length);
    tiles[BOARD_SIZE] = '\0';

    return make_row(tiles);
}

/* Word pattern table construction helpers */

/*
    WordPatternTable is a compact, searchable index:
      buckets_by_length[length][fixed_mask] -> sorted WordPattern rows
      WordPattern                          -> range in table.anagram_groups
      WordPatternAnagramGroup              -> range in table.word_indices

    fixed_mask bits mark board-supplied letters. The missing letters are stored
    as a sorted anagram key, limited to the seven letters a rack can supply.
*/

struct PatternRecord {
    PatternRow fixed_tiles;
    PatternBytes rack_letters;
    uint32_t word_index;
};

static int init_word_pattern_buckets(WordPatternTable *table)
{
    if (table == NULL) {
        return 0;
    }

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

static int reserve_pattern_records_for_words(std::vector<PatternRecord> *records, const WordTable *words)
{
    size_t record_count = 0;

    for (size_t word_index = 0; word_index < words->count; ++word_index) {
        size_t word_record_count = pattern_record_count_for_word(words->words[word_index]);

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
        row.keepMask |= put_byte_128(UINT8_MAX, index, BOARD_SIZE);
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
    uint16_t pattern_count;
    uint16_t active_bits;
    PatternBytes packed_word = 0;

    if (word_length == 0 || word_length > BOARD_SIZE) {
        return 1;
    }

    /*
        pattern_mask marks the wildcard letters. A pattern is only useful when
        it has 1..7 wildcard letters because the rack can supply at most seven.
    */
    pattern_count = (uint16_t)(UINT16_C(1) << word_length);
    active_bits = low_bit_mask_u16(word_length);
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

static int build_pattern_records_from_words(std::vector<PatternRecord> *records, const WordTable *words)
{
    if (!reserve_pattern_records_for_words(records, words)) {
        return 0;
    }

    for (size_t word_index = 0; word_index < words->count; ++word_index) {
        if (!append_pattern_records_for_word(records, words->words[word_index], (uint32_t)word_index)) {
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

static uint64_t word_table_fingerprint(const WordTable *words)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    if (words == NULL || words->words == NULL) {
        return hash;
    }

    for (size_t word_index = 0; word_index < words->count; ++word_index) {
        const unsigned char *word = reinterpret_cast<const unsigned char *>(words->words[word_index]);

        for (size_t char_index = 0; word[char_index] != '\0'; ++char_index) {
            hash ^= word[char_index];
            hash *= UINT64_C(1099511628211);
        }

        hash ^= 0;
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

/* Word table construction */

static int add_unique_word(WordTable *table, const char *line)
{
    WordSetHash *hash_state = static_cast<WordSetHash *>(table->hash_state);
    char *word;

    if (hash_state->find(line) != hash_state->end()) {
        return 1;
    }

    word = copy_word(line);
    if (word == NULL) {
        return 0;
    }

    try {
        hash_state->insert(word);
    } catch (...) {
        free(word);
        return 0;
    }

    table->words[table->count] = word;
    table->count++;

    return 1;
}

/* Public word table API */

WordTable words_from_file(const char *file_path)
{
    WordTable table = {};
    FILE *file = fopen(file_path, "r");
    size_t max_word_count;
    char *line = NULL;
    size_t line_capacity = 0;

    if (file == NULL) {
        return table;
    }

    max_word_count = count_words(file);
    if (max_word_count == 0) {
        fclose(file);
        return table;
    }

    table.words = static_cast<char **>(calloc(max_word_count, sizeof(table.words[0])));
    if (table.words == NULL) {
        fclose(file);
        return table;
    }

    table.hash_state = new (std::nothrow) WordSetHash();
    if (table.hash_state == NULL) {
        free(table.words);
        fclose(file);
        return WordTable{};
    }

    try {
        static_cast<WordSetHash *>(table.hash_state)->reserve(max_word_count + (max_word_count / 4) + 1);
    } catch (...) {
        delete static_cast<WordSetHash *>(table.hash_state);
        free(table.words);
        fclose(file);
        return WordTable{};
    }

    while (getline(&line, &line_capacity, file) != -1) {
        trim_line_ending(line);
        uppercase_alpha_chars(line);
        if (line[0] == '\0') {
            continue;
        }

        if (!add_unique_word(&table, line)) {
            word_table_destroy(&table);
            free(line);
            fclose(file);
            return WordTable{};
        }
    }

    free(line);
    fclose(file);

    return table;
}

int word_table_contains(const WordTable *table, const char *word)
{
    const WordSetHash *hash;

    if (table == NULL || table->words == NULL || table->hash_state == NULL || word == NULL) {
        return 0;
    }

    hash = static_cast<const WordSetHash *>(table->hash_state);

    return hash->find(word) != hash->end();
}

void word_table_print(const WordTable *table)
{
    if (table == NULL || table->words == NULL) {
        return;
    }

    for (size_t word_index = 0; word_index < table->count; ++word_index) {
        printf("%s\n", table->words[word_index]);
    }
}

/* Public word start row API */

WordStartRowTable word_start_row_table_from_word_table(const WordTable *words)
{
    WordStartRowTable table = {};

    if (words == NULL || words->words == NULL || words->count == 0) {
        return table;
    }

    table.entries = static_cast<WordStartRowEntry *>(calloc(words->count, sizeof(table.entries[0])));
    if (table.entries == NULL) {
        return table;
    }

    table.hash_state = new (std::nothrow) WordStartRowHash();
    if (table.hash_state == NULL) {
        free(table.entries);
        return WordStartRowTable{};
    }

    try {
        static_cast<WordStartRowHash *>(table.hash_state)->reserve(words->count + (words->count / 4) + 1);
    } catch (...) {
        delete static_cast<WordStartRowHash *>(table.hash_state);
        free(table.entries);
        return WordStartRowTable{};
    }

    for (size_t word_index = 0; word_index < words->count; ++word_index) {
        WordStartRowEntry *entry = &table.entries[word_index];
        size_t word_length = strlen(words->words[word_index]);

        entry->word = words->words[word_index];
        entry->word_length = (uint8_t)word_length;

        for (size_t start = 0; start < BOARD_SIZE; ++start) {
            if (word_length <= BOARD_SIZE - start) {
                entry->rows[start] = make_word_start_row_at(entry->word, start);
                entry->valid_starts |= (uint16_t)(UINT16_C(1) << start);
            }
        }

        try {
            static_cast<WordStartRowHash *>(table.hash_state)->emplace(entry->word, entry);
        } catch (...) {
            word_start_row_table_destroy(&table);
            return WordStartRowTable{};
        }

        table.count++;
    }

    return table;
}

const Row *word_start_row_table_get(const WordStartRowTable *table, const char *word, size_t start)
{
    const WordStartRowHash *hash;
    WordStartRowHash::const_iterator found;
    WordStartRowEntry *entry;

    if (table == NULL || table->entries == NULL || table->hash_state == NULL || word == NULL || start >= BOARD_SIZE) {
        return NULL;
    }

    hash = static_cast<const WordStartRowHash *>(table->hash_state);
    found = hash->find(word);

    if (found == hash->end()) {
        return NULL;
    }

    entry = found->second;
    if ((entry->valid_starts & (uint16_t)(UINT16_C(1) << start)) == 0) {
        return NULL;
    }

    return &entry->rows[start];
}

WordIndexStartRowTable word_index_start_row_table_from_word_table(const WordTable *words)
{
    WordIndexStartRowTable table = {};

    if (words == NULL || words->words == NULL || words->count == 0) {
        return table;
    }

    table.entries = static_cast<WordIndexStartRowEntry *>(calloc(words->count, sizeof(table.entries[0])));
    if (table.entries == NULL) {
        return table;
    }

    for (size_t word_index = 0; word_index < words->count; ++word_index) {
        WordIndexStartRowEntry *entry = &table.entries[word_index];
        size_t word_length = strlen(words->words[word_index]);

        entry->word_length = (uint8_t)word_length;

        for (size_t start = 0; start < BOARD_SIZE; ++start) {
            if (word_length <= BOARD_SIZE - start) {
                entry->rows[start] = make_word_start_row_at(words->words[word_index], start);
                entry->valid_starts |= (uint16_t)(UINT16_C(1) << start);
            }
        }

        table.count++;
    }

    return table;
}

const Row *word_index_start_row_table_get(const WordIndexStartRowTable *table, size_t word_index, size_t start)
{
    const WordIndexStartRowEntry *entry;

    if (table == NULL || table->entries == NULL || word_index >= table->count || start >= BOARD_SIZE) {
        return NULL;
    }

    entry = &table->entries[word_index];
    if ((entry->valid_starts & (uint16_t)(UINT16_C(1) << start)) == 0) {
        return NULL;
    }

    return &entry->rows[start];
}

void word_start_row_table_print(const WordStartRowTable *table)
{
    if (table == NULL || table->entries == NULL) {
        return;
    }

    for (size_t entry_index = 0; entry_index < table->count; ++entry_index) {
        const WordStartRowEntry *entry = &table->entries[entry_index];

        for (size_t start = 0; start < BOARD_SIZE; ++start) {
            const Row *row;

            if ((entry->valid_starts & (uint16_t)(UINT16_C(1) << start)) == 0) {
                continue;
            }

            row = &entry->rows[start];
            printf("%s start %2zu ", entry->word, start);
            print_bits_grouped(stdout, row->tiles, BOARD_SIZE * ROW_TILE_BITS, ROW_TILE_BITS, 0);
            putchar('\n');
        }
    }
}

/* Public word pattern API */

WordPatternTable word_pattern_table_from_word_table(const WordTable *words)
{
    WordPatternTable table;
    std::vector<PatternRecord> records;

    if (!init_word_pattern_buckets(&table)) {
        return {};
    }

    if (words == NULL || words->words == NULL || words->count == 0) {
        return table;
    }

    if (words->count > (size_t)std::numeric_limits<uint32_t>::max()) {
        return {};
    }

    if (!build_pattern_records_from_words(&records, words)) {
        return {};
    }

    std::sort(records.begin(), records.end(), pattern_record_less);

    if (!pack_sorted_pattern_records(&table, records)) {
        return {};
    }

    table.dictionary_fingerprint = word_table_fingerprint(words);
    table.dictionary_word_count = words->count;

    return table;
}

int word_pattern_table_is_empty(const WordPatternTable *table)
{
    if (table == NULL) {
        return 1;
    }

    for (const WordPatternBucketsByMask &length_buckets : table->buckets_by_length) {
        for (const WordPatternBucket &bucket : length_buckets) {
            if (!bucket.empty()) {
                return 0;
            }
        }
    }

    return 1;
}

size_t word_pattern_table_largest_binary_search_domain(const WordPatternTable *table)
{
    size_t largest = 0;

    if (table == NULL) {
        return 0;
    }

    for (const WordPatternBucketsByMask &length_buckets : table->buckets_by_length) {
        for (const WordPatternBucket &bucket : length_buckets) {
            if (bucket.size() > largest) {
                largest = bucket.size();
            }
        }
    }

    return largest;
}

const WordPattern *word_pattern_table_get(const WordPatternTable *table, const PatternRow *pattern)
{
    if (table == NULL || pattern == NULL || pattern->length == 0 || pattern->length > BOARD_SIZE) {
        return NULL;
    }

    if (table->buckets_by_length.size() <= pattern->length ||
        table->buckets_by_length[pattern->length].size() <= pattern->occupiedMask ||
        pattern->occupiedMask > low_bit_mask_u16(pattern->length)) {
        return NULL;
    }

    const WordPatternBucket &bucket = table->buckets_by_length[pattern->length][pattern->occupiedMask];

    auto iter = std::lower_bound(
        bucket.begin(),
        bucket.end(),
        pattern->tiles,
        [](const WordPattern &entry, PatternBytes value) {
            return entry.fixed_tiles < value;
        }
    );

    if (iter == bucket.end() || iter->fixed_tiles != pattern->tiles) {
        return NULL;
    }

    return &*iter;
}

const WordPatternAnagramGroup *word_pattern_entry_get_anagram(const WordPatternTable *table, const WordPattern *entry, PatternBytes anagram)
{
    if (table == NULL || entry == NULL) {
        return NULL;
    }

    if ((size_t)entry->anagrams.start + entry->anagrams.count > table->anagram_groups.size()) {
        return NULL;
    }

    auto begin = table->anagram_groups.begin() + entry->anagrams.start;
    auto end = begin + entry->anagrams.count;
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

const char *word_pattern_table_word_at(const WordPatternTable *table, const WordTable *words, const WordPatternAnagramGroup *entry, size_t offset)
{
    if (table == NULL || words == NULL || words->words == NULL || entry == NULL || offset >= entry->word_indices.count) {
        return NULL;
    }

    if ((size_t)entry->word_indices.start + entry->word_indices.count > table->word_indices.size()) {
        return NULL;
    }

    uint32_t word_index = table->word_indices[(size_t)entry->word_indices.start + offset];
    if (word_index >= words->count) {
        return NULL;
    }

    return words->words[word_index];
}

int word_pattern_anagram_contains_word(const WordPatternTable *table, const WordTable *words, const WordPatternAnagramGroup *entry, const char *word)
{
    if (word == NULL) {
        return 0;
    }

    for (size_t offset = 0; entry != NULL && offset < entry->word_indices.count; ++offset) {
        const char *candidate = word_pattern_table_word_at(table, words, entry, offset);

        if (candidate != NULL && strcmp(candidate, word) == 0) {
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

int word_pattern_table_save(const WordPatternTable *table, const WordTable *words, const char *cache_path)
{
    static const char magic[8] = {'W', 'P', 'T', 'I', 'D', 'X', '1', '\0'};
    FILE *file;
    uint64_t word_count;
    uint64_t bucket_count = 0;
    uint64_t anagram_count;
    uint64_t word_index_count;
    uint64_t fingerprint;

    if (table == NULL || words == NULL || words->words == NULL || cache_path == NULL) {
        return 0;
    }

    if (table->anagram_groups.size() > (size_t)std::numeric_limits<uint32_t>::max() ||
        table->word_indices.size() > (size_t)std::numeric_limits<uint32_t>::max()) {
        return 0;
    }

    if (!count_serialized_pattern_buckets(table, &bucket_count)) {
        return 0;
    }

    word_count = (uint64_t)words->count;
    anagram_count = (uint64_t)table->anagram_groups.size();
    word_index_count = (uint64_t)table->word_indices.size();
    fingerprint = word_table_fingerprint(words);

    file = fopen(cache_path, "wb");
    if (file == NULL) {
        return 0;
    }

    int ok =
        write_exact(file, magic, sizeof(magic)) &&
        write_u32(file, 3) &&
        write_u32(file, BOARD_SIZE) &&
        write_u32(file, UINT32_C(0x01020304)) &&
        write_u64(file, word_count) &&
        write_u64(file, fingerprint) &&
        write_u64(file, bucket_count) &&
        write_u64(file, anagram_count) &&
        write_u64(file, word_index_count);

    for (size_t length = 0; ok && length < table->buckets_by_length.size(); ++length) {
        const WordPatternBucketsByMask &length_buckets = table->buckets_by_length[length];

        for (size_t fixed_mask = 0; ok && fixed_mask < length_buckets.size(); ++fixed_mask) {
            const WordPatternBucket &bucket = length_buckets[fixed_mask];

            if (bucket.empty()) {
                continue;
            }

            ok = write_pattern_bucket(file, length, fixed_mask, bucket);
        }
    }

    for (size_t index = 0; ok && index < table->anagram_groups.size(); ++index) {
        const WordPatternAnagramGroup &entry = table->anagram_groups[index];
        ok =
            write_pattern_bytes(file, entry.rack_letters) &&
            write_u32(file, entry.word_indices.start) &&
            write_u32(file, entry.word_indices.count);
    }

    for (size_t index = 0; ok && index < table->word_indices.size(); ++index) {
        ok = write_u32(file, table->word_indices[index]);
    }

    if (fclose(file) != 0) {
        return 0;
    }

    return ok;
}

WordPatternTable word_pattern_table_load(const WordTable *words, const char *cache_path)
{
    static const char expected_magic[8] = {'W', 'P', 'T', 'I', 'D', 'X', '1', '\0'};
    WordPatternTable table;
    FILE *file;
    char magic[8];
    uint32_t version;
    uint32_t board_size;
    uint32_t endian_marker;
    uint64_t word_count;
    uint64_t fingerprint;
    uint64_t bucket_count;
    uint64_t anagram_count;
    uint64_t word_index_count;

    if (words == NULL || words->words == NULL || cache_path == NULL) {
        return table;
    }

    file = fopen(cache_path, "rb");
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
        word_count == (uint64_t)words->count &&
        fingerprint == word_table_fingerprint(words) &&
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
        ok = read_u32(file, &table.word_indices[index]) && table.word_indices[index] < words->count;
    }

    fclose(file);

    if (!ok) {
        return WordPatternTable{};
    }

    table.dictionary_fingerprint = fingerprint;
    table.dictionary_word_count = (size_t)word_count;
    return table;
}

void word_pattern_table_print(const WordPatternTable *table, const WordTable *words)
{
    if (table == NULL || words == NULL) {
        return;
    }

    for (const WordPatternBucketsByMask &length_buckets : table->buckets_by_length) {
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
                    const WordPatternAnagramGroup &anagram_entry = table->anagram_groups[(size_t)pattern_entry.anagrams.start + anagram_offset];
                    const char *letters = reinterpret_cast<const char *>(&anagram_entry.rack_letters);

                    printf("  letters \"%.*s\":", 7, letters);

                    for (uint32_t word_offset = 0; word_offset < anagram_entry.word_indices.count; ++word_offset) {
                        const char *word = word_pattern_table_word_at(table, words, &anagram_entry, word_offset);

                        if (word != NULL) {
                            printf(" %s", word);
                        }
                    }

                    putchar('\n');
                }
            }
        }
    }
}

/* Destructors */

void word_table_destroy(WordTable *table)
{
    if (table == NULL) {
        return;
    }

    if (table->hash_state != NULL) {
        delete static_cast<WordSetHash *>(table->hash_state);
    }

    free_words(table->words, table->count);
    table->words = NULL;
    table->count = 0;
    table->hash_state = NULL;
}

void word_start_row_table_destroy(WordStartRowTable *table)
{
    if (table == NULL) {
        return;
    }

    if (table->hash_state != NULL) {
        delete static_cast<WordStartRowHash *>(table->hash_state);
    }

    free(table->entries);
    table->entries = NULL;
    table->count = 0;
    table->hash_state = NULL;
}

void word_index_start_row_table_destroy(WordIndexStartRowTable *table)
{
    if (table == NULL) {
        return;
    }

    free(table->entries);
    table->entries = NULL;
    table->count = 0;
}
