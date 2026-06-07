#include "board.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char temp_board_path[128];
static char temp_config_path[128];
static uint16_t index_to_config[MAX_NUMBER_OF_START_CONFIGS];
static uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE];

static void setup(void)
{
    snprintf(temp_board_path, sizeof(temp_board_path), "/tmp/scrabble_board_%ld.csv", (long)getpid());
    snprintf(temp_config_path, sizeof(temp_config_path), "/tmp/scrabble_board_config_%ld.csv", (long)getpid());
    memset(index_to_config, 0, sizeof(index_to_config));
    init_config_maps(index_to_config, config_to_index);

    FILE *file = fopen(temp_board_path, "w");
    assert(file != NULL);
    fclose(file);

    file = fopen(temp_config_path, "w");
    assert(file != NULL);
    fclose(file);
}

static void cleanup_after_each(void)
{
    remove(temp_board_path);
    remove(temp_config_path);
}

static void write_csv_row(FILE *file, const char tiles[BOARD_SIZE + 1])
{
    for (int col_index = 0; col_index < BOARD_SIZE; ++col_index) {
        if (col_index > 0) {
            fputc(',', file);
        }

        fputc(tiles[col_index], file);
    }

    fputc('\n', file);
}

static void write_empty_config(void)
{
    FILE *file = fopen(temp_config_path, "w");
    assert(file != NULL);

    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        write_csv_row(file, "...............");
    }

    fclose(file);
}

static void write_config_with_first_row(const char *first_row)
{
    FILE *file = fopen(temp_config_path, "w");
    assert(file != NULL);

    fputs(first_row, file);
    fputc('\n', file);

    for (int row_index = 1; row_index < BOARD_SIZE; ++row_index) {
        write_csv_row(file, "...............");
    }

    fclose(file);
}

static void write_board_with_first_row(const char first_row[BOARD_SIZE + 1])
{
    FILE *file = fopen(temp_board_path, "w");
    assert(file != NULL);

    write_csv_row(file, first_row);

    for (int row_index = 1; row_index < BOARD_SIZE; ++row_index) {
        write_csv_row(file, "...............");
    }

    fclose(file);
    write_empty_config();
}

static void write_text_file(const char *contents)
{
    FILE *file = fopen(temp_board_path, "w");
    assert(file != NULL);
    fputs(contents, file);
    fclose(file);
}

static void assert_board_is_zeroed(Board board)
{
    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        assert(board.rows[row_index].first3Tiles == 0);
        assert(board.rows[row_index].last12Tiles == 0);
        assert(board.wordsConfigs[row_index] == 0);
    }
}

static void loads_letters_from_csv_board(void)
{
    const char first_row[BOARD_SIZE + 1] = "ABCDEFGHIJKLMNO";
    Row expected_first_row = make_row(first_row);

    write_board_with_first_row(first_row);

    Board board = board_from_csv(temp_board_path, temp_config_path, config_to_index);

    assert(board.rows[0].first3Tiles == expected_first_row.first3Tiles);
    assert(board.rows[0].last12Tiles == expected_first_row.last12Tiles);

    for (int row_index = 1; row_index < BOARD_SIZE; ++row_index) {
        assert(board.rows[row_index].first3Tiles == UINT16_MAX);
        assert(board.rows[row_index].last12Tiles == UINT64_MAX);
    }
}

static void keeps_empty_tiles_as_blank_values(void)
{
    const char first_row[BOARD_SIZE + 1] = ".Z.A...........";
    Row expected_first_row = make_row(first_row);

    write_board_with_first_row(first_row);

    Board board = board_from_csv(temp_board_path, temp_config_path, config_to_index);

    assert(board.rows[0].first3Tiles == expected_first_row.first3Tiles);
    assert(board.rows[0].last12Tiles == expected_first_row.last12Tiles);
}

static void returns_zeroed_board_when_first_csv_row_is_malformed(void)
{
    write_text_file("A,B,C\n");

    Board board = board_from_csv(temp_board_path, temp_config_path, config_to_index);

    assert_board_is_zeroed(board);
}

static void loads_word_config_indices_and_lengths(void)
{
    uint16_t config_mask = (uint16_t)(1u << 7);
    uint16_t lookup_index = config_to_index[config_mask];

    write_board_with_first_row("...............");
    write_config_with_first_row(".,.,.,.,.,.,.,S,2,.,.,.,.,.,.");

    Board board = board_from_csv(temp_board_path, temp_config_path, config_to_index);

    assert(board.wordsConfigs[0] == ((lookup_index & WORD_CONFIG_INDEX_MASK) | (UINT32_C(2) << WORD_CONFIG_INDEX_BITS)));
}

static void loads_multi_digit_word_config_lengths(void)
{
    uint16_t config_mask = (uint16_t)(1u << 0);
    uint16_t lookup_index = config_to_index[config_mask];

    write_board_with_first_row("...............");
    write_config_with_first_row("S,15,.,.,.,.,.,.,.,.,.,.,.,.,.");

    Board board = board_from_csv(temp_board_path, temp_config_path, config_to_index);

    assert(board.wordsConfigs[0] == ((lookup_index & WORD_CONFIG_INDEX_MASK) | (UINT32_C(15) << WORD_CONFIG_INDEX_BITS)));
}

static void loads_multiple_word_config_starts_and_lengths(void)
{
    uint16_t config_mask = (uint16_t)((1u << 2) | (1u << 11));
    uint16_t lookup_index = config_to_index[config_mask];

    write_board_with_first_row("...............");
    write_config_with_first_row(".,.,S,6,.,.,.,.,.,.,.,S,3,.,.");

    Board board = board_from_csv(temp_board_path, temp_config_path, config_to_index);

    assert(board.wordsConfigs[0] == (
        (lookup_index & WORD_CONFIG_INDEX_MASK)
        | (UINT32_C(6) << WORD_CONFIG_INDEX_BITS)
        | (UINT32_C(3) << (WORD_CONFIG_INDEX_BITS + WORD_CONFIG_LENGTH_BITS))
    ));
}

static void returns_zeroed_board_when_config_row_is_malformed(void)
{
    write_board_with_first_row("...............");
    write_config_with_first_row("S,15,.,.,.,.,.,.,.,.,.,.,.,.,.,.");

    Board board = board_from_csv(temp_board_path, temp_config_path, config_to_index);

    assert_board_is_zeroed(board);
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
    run_test("loads_letters_from_csv_board", loads_letters_from_csv_board);
    run_test("keeps_empty_tiles_as_blank_values", keeps_empty_tiles_as_blank_values);
    run_test("returns_zeroed_board_when_first_csv_row_is_malformed", returns_zeroed_board_when_first_csv_row_is_malformed);
    run_test("loads_word_config_indices_and_lengths", loads_word_config_indices_and_lengths);
    run_test("loads_multi_digit_word_config_lengths", loads_multi_digit_word_config_lengths);
    run_test("loads_multiple_word_config_starts_and_lengths", loads_multiple_word_config_starts_and_lengths);
    run_test("returns_zeroed_board_when_config_row_is_malformed", returns_zeroed_board_when_config_row_is_malformed);

    return 0;
}
