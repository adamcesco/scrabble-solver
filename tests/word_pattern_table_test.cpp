#include "dictionary.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char temp_dictionary_path[128];
static char temp_cache_path[128];

static void setup(void)
{
    snprintf(temp_dictionary_path, sizeof(temp_dictionary_path), "/tmp/scrabble_word_patterns_%ld.txt", (long)getpid());
    snprintf(temp_cache_path, sizeof(temp_cache_path), "/tmp/scrabble_word_patterns_%ld.cache", (long)getpid());
}

static void cleanup_after_each(void)
{
    remove(temp_dictionary_path);
    remove(temp_cache_path);
}

static void write_text_file(const char *contents)
{
    FILE *file = fopen(temp_dictionary_path, "w");
    assert(file != NULL);
    fputs(contents, file);
    fclose(file);
}

static void test_write_u32(FILE *file, uint32_t value)
{
    assert(fwrite(&value, sizeof(value), 1, file) == 1);
}

static void test_write_u64(FILE *file, uint64_t value)
{
    assert(fwrite(&value, sizeof(value), 1, file) == 1);
}

static WordTable load_dictionary(const char *contents)
{
    write_text_file(contents);
    return WordTable::from_file(temp_dictionary_path);
}

static PatternBytes test_pattern_byte(uint8_t value, unsigned slot)
{
    return put_byte_128(value, slot, BOARD_SIZE);
}

static uint8_t test_pattern_byte_at(PatternBytes value, unsigned slot)
{
    return get_byte_128(value, slot, BOARD_SIZE);
}

static PatternRow pattern_row_for_pattern(const char *pattern)
{
    PatternRow row = {};

    for (uint8_t index = 0; pattern[index] != '\0'; ++index) {
        row.length++;

        if (pattern[index] != ' ') {
            row.tiles |= test_pattern_byte((uint8_t)pattern[index], index);
            row.occupiedMask |= (uint16_t)(UINT16_C(1) << index);
        }
    }

    return row;
}

static PatternBytes packed_letters(const char *letters)
{
    PatternBytes packed = 0;

    for (uint8_t index = 0; letters[index] != '\0'; ++index) {
        packed |= test_pattern_byte((uint8_t)letters[index], index);
    }

    return packed;
}

static const WordPatternAnagramGroup *get_anagram_entry(const WordPatternTable *patterns, const char *pattern, const char *letters)
{
    PatternRow pattern_row = pattern_row_for_pattern(pattern);
    const WordPattern *pattern_entry = patterns->get(pattern_row);

    assert(pattern_entry != NULL);
    return patterns->get_anagram(*pattern_entry, packed_letters(letters));
}

static void assert_anagram_words(const WordPatternTable *patterns, const WordTable *words, const WordPatternAnagramGroup *entry, const char *expected[], size_t expected_count)
{
    assert(entry != NULL);
    assert(entry->word_indices.count == expected_count);

    for (size_t word_index = 0; word_index < expected_count; ++word_index) {
        const char *word = patterns->word_at(*words, *entry, word_index);

        assert(word != NULL);
        assert(strcmp(word, expected[word_index]) == 0);
        assert(patterns->anagram_contains_word(*words, *entry, expected[word_index]));
    }
}

static void returns_words_matching_fixed_position_wildcards_and_anagram_letters(void)
{
    WordTable words = load_dictionary("APPLE\nAPPLY\nAMPLE\nBERRY\n");
    WordPatternTable patterns = WordPatternTable::from_words(words);
    const char *ae_expected[] = {"APPLE"};
    const char *ay_expected[] = {"APPLY"};

    assert_anagram_words(&patterns, &words, get_anagram_entry(&patterns, " PPL ", "AE"), ae_expected, 1);
    assert_anagram_words(&patterns, &words, get_anagram_entry(&patterns, " PPL ", "AY"), ay_expected, 1);

    words.destroy();
}

static void enforces_exact_pattern_length(void)
{
    WordTable words = load_dictionary("APP\nAPPLE\nAPPLES\n");
    WordPatternTable patterns = WordPatternTable::from_words(words);
    const char *expected[] = {"APPLE"};

    assert_anagram_words(&patterns, &words, get_anagram_entry(&patterns, "APP  ", "EL"), expected, 1);

    words.destroy();
}

static void returns_all_words_that_share_a_pattern_and_anagram_key_in_dictionary_order(void)
{
    WordTable words = load_dictionary("CAT\nTAC\nACT\nBAR\nCAB\nDOG\n");
    WordPatternTable patterns = WordPatternTable::from_words(words);
    const char *middle_a_expected[] = {"CAT", "TAC"};
    const char *all_space_expected[] = {"CAT", "TAC", "ACT"};

    assert_anagram_words(&patterns, &words, get_anagram_entry(&patterns, " A ", "CT"), middle_a_expected, 2);
    assert_anagram_words(&patterns, &words, get_anagram_entry(&patterns, "   ", "ACT"), all_space_expected, 3);

    words.destroy();
}

static void indexes_lowercase_dictionary_words_as_uppercase_anagrams(void)
{
    WordTable words = load_dictionary("chai\nciao\n");
    WordPatternTable patterns = WordPatternTable::from_words(words);
    const char *blank_expected[] = {"CHAI"};
    const char *fixed_h_expected[] = {"CHAI"};

    assert_anagram_words(&patterns, &words, get_anagram_entry(&patterns, "    ", "ACHI"), blank_expected, 1);
    assert_anagram_words(&patterns, &words, get_anagram_entry(&patterns, " H  ", "ACI"), fixed_h_expected, 1);

    words.destroy();
}

static void requires_sorted_anagram_keys(void)
{
    WordTable words = load_dictionary("CHAI\n");
    WordPatternTable patterns = WordPatternTable::from_words(words);
    PatternRow blank_pattern = pattern_row_for_pattern("    ");
    const WordPattern *pattern_entry = patterns.get(blank_pattern);

    assert(pattern_entry != NULL);
    assert(patterns.get_anagram(*pattern_entry, packed_letters("ACHI")) != NULL);
    assert(patterns.get_anagram(*pattern_entry, packed_letters("CHAI")) == NULL);

    words.destroy();
}

static void returns_null_for_zero_wildcard_patterns(void)
{
    WordTable words = load_dictionary("APPLE\nAPPLY\n");
    WordPatternTable patterns = WordPatternTable::from_words(words);
    PatternRow apple = pattern_row_for_pattern("APPLE");
    PatternRow apply = pattern_row_for_pattern("APPLY");

    assert(patterns.get(apple) == NULL);
    assert(patterns.get(apply) == NULL);

    words.destroy();
}

static void skips_patterns_with_more_than_seven_zero_mask_bits(void)
{
    WordTable words = load_dictionary("ELEPHANT\n");
    WordPatternTable patterns = WordPatternTable::from_words(words);
    PatternRow no_wildcards = pattern_row_for_pattern("ELEPHANT");
    PatternRow one_wildcard = pattern_row_for_pattern(" LEPHANT");

    assert(patterns.get(no_wildcards) == NULL);
    assert(patterns.get(one_wildcard) != NULL);

    words.destroy();
}

static void returns_null_for_no_packed_match(void)
{
    WordTable words = load_dictionary("CAT\n");
    WordPatternTable patterns = WordPatternTable::from_words(words);
    PatternRow match = pattern_row_for_pattern("C  ");
    PatternRow no_match = pattern_row_for_pattern(" Z ");
    const WordPattern *pattern_entry = patterns.get(match);

    assert(patterns.get(no_match) == NULL);
    assert(pattern_entry != NULL);
    assert(patterns.get_anagram(*pattern_entry, packed_letters("ZZ")) == NULL);

    words.destroy();
}

static void encodes_pattern_blanks_as_zero_bytes(void)
{
    WordTable words = load_dictionary("APPLE\n");
    WordPatternTable patterns = WordPatternTable::from_words(words);
    PatternRow pattern = pattern_row_for_pattern(" PPL ");
    const WordPattern *pattern_entry = patterns.get(pattern);
    assert(pattern_entry != NULL);
    assert(pattern.length == 5);
    assert(pattern.occupiedMask == UINT16_C(0x000E));
    assert(test_pattern_byte_at(pattern.tiles, 0) == 0);
    assert(test_pattern_byte_at(pattern.tiles, 1) == 'P');
    assert(test_pattern_byte_at(pattern.tiles, 2) == 'P');
    assert(test_pattern_byte_at(pattern.tiles, 3) == 'L');
    assert(test_pattern_byte_at(pattern.tiles, 4) == 0);
    assert(pattern_entry->fixed_tiles == pattern.tiles);

    words.destroy();
}

static void returns_empty_table_for_empty_word_tables(void)
{
    WordTable words = {};
    WordPatternTable patterns = WordPatternTable::from_words(words);

    assert(patterns.is_empty());
}

static void returns_largest_binary_search_domain(void)
{
    WordTable words = load_dictionary("CAT\nTAC\nACT\nBAR\n");
    WordPatternTable patterns = WordPatternTable::from_words(words);

    assert(patterns.largest_binary_search_domain() == 4);

    words.destroy();
}

static void saves_and_loads_word_pattern_cache(void)
{
    WordTable words = load_dictionary("CAT\nTAC\nACT\nBAR\n");
    WordPatternTable patterns = WordPatternTable::from_words(words);
    const char *expected[] = {"CAT", "TAC", "ACT"};

    assert(patterns.save(words, temp_cache_path) == 1);

    WordPatternTable loaded = WordPatternTable::load(words, temp_cache_path);
    assert(!loaded.is_empty());
    assert_anagram_words(&loaded, &words, get_anagram_entry(&loaded, "   ", "ACT"), expected, 3);

    words.destroy();
}

static void rejects_cache_for_different_dictionary(void)
{
    WordTable words = load_dictionary("CAT\nTAC\nACT\n");
    WordPatternTable patterns = WordPatternTable::from_words(words);

    assert(patterns.save(words, temp_cache_path) == 1);
    words.destroy();

    WordTable different_words = load_dictionary("CAT\nACT\nTAC\n");
    WordPatternTable loaded = WordPatternTable::load(different_words, temp_cache_path);

    assert(loaded.is_empty());

    different_words.destroy();
}

static void returns_empty_table_for_missing_or_invalid_cache(void)
{
    WordTable words = load_dictionary("CAT\n");
    WordPatternTable missing = WordPatternTable::load(words, "/tmp/scrabble_missing_word_pattern_cache.bin");

    assert(missing.is_empty());

    FILE *file = fopen(temp_cache_path, "w");
    assert(file != NULL);
    fputs("not a cache", file);
    fclose(file);

    WordPatternTable invalid = WordPatternTable::load(words, temp_cache_path);

    assert(invalid.is_empty());

    words.destroy();
}

static void rejects_old_word_pattern_cache_version(void)
{
    static const char magic[8] = {'W', 'P', 'T', 'I', 'D', 'X', '1', '\0'};
    WordTable words = load_dictionary("CAT\n");
    FILE *file = fopen(temp_cache_path, "wb");
    assert(file != NULL);

    assert(fwrite(magic, sizeof(magic), 1, file) == 1);
    test_write_u32(file, 1);
    test_write_u32(file, BOARD_SIZE);
    test_write_u32(file, UINT32_C(0x01020304));
    test_write_u64(file, words.count);
    test_write_u64(file, 0);
    test_write_u64(file, 0);
    test_write_u64(file, 0);
    test_write_u64(file, 0);
    fclose(file);

    WordPatternTable old_cache = WordPatternTable::load(words, temp_cache_path);

    assert(old_cache.is_empty());

    words.destroy();
}

static void rejects_previous_zero_space_cache_version(void)
{
    WordTable words = load_dictionary("CAT\n");
    WordPatternTable patterns = WordPatternTable::from_words(words);

    assert(patterns.save(words, temp_cache_path) == 1);

    FILE *file = fopen(temp_cache_path, "r+b");
    assert(file != NULL);
    assert(fseek(file, 8, SEEK_SET) == 0);
    test_write_u32(file, 2);
    fclose(file);

    WordPatternTable previous_cache = WordPatternTable::load(words, temp_cache_path);

    assert(previous_cache.is_empty());

    words.destroy();
}

static void run_test(const char *name, void (*test)(void))
{
    setup();
    test();
    cleanup_after_each();
    printf("PASS %s\n", name);
}

int main(void)
{
    run_test("returns_words_matching_fixed_position_wildcards_and_anagram_letters", returns_words_matching_fixed_position_wildcards_and_anagram_letters);
    run_test("enforces_exact_pattern_length", enforces_exact_pattern_length);
    run_test("returns_all_words_that_share_a_pattern_and_anagram_key_in_dictionary_order", returns_all_words_that_share_a_pattern_and_anagram_key_in_dictionary_order);
    run_test("indexes_lowercase_dictionary_words_as_uppercase_anagrams", indexes_lowercase_dictionary_words_as_uppercase_anagrams);
    run_test("requires_sorted_anagram_keys", requires_sorted_anagram_keys);
    run_test("returns_null_for_zero_wildcard_patterns", returns_null_for_zero_wildcard_patterns);
    run_test("skips_patterns_with_more_than_seven_zero_mask_bits", skips_patterns_with_more_than_seven_zero_mask_bits);
    run_test("returns_null_for_no_packed_match", returns_null_for_no_packed_match);
    run_test("encodes_pattern_blanks_as_zero_bytes", encodes_pattern_blanks_as_zero_bytes);
    run_test("returns_empty_table_for_empty_word_tables", returns_empty_table_for_empty_word_tables);
    run_test("returns_largest_binary_search_domain", returns_largest_binary_search_domain);
    run_test("saves_and_loads_word_pattern_cache", saves_and_loads_word_pattern_cache);
    run_test("rejects_cache_for_different_dictionary", rejects_cache_for_different_dictionary);
    run_test("returns_empty_table_for_missing_or_invalid_cache", returns_empty_table_for_missing_or_invalid_cache);
    run_test("rejects_old_word_pattern_cache_version", rejects_old_word_pattern_cache_version);
    run_test("rejects_previous_zero_space_cache_version", rejects_previous_zero_space_cache_version);

    return 0;
}
