#include "board.h"

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char temp_board_path[128];

static void setup_all(void)
{
    snprintf(temp_board_path, sizeof(temp_board_path), "/tmp/scrabble_board_%ld.csv", (long)getpid());

    FILE *file = fopen(temp_board_path, "w");
    assert(file != NULL);
    fclose(file);
}

static void cleanup_after_each(void)
{
    remove(temp_board_path);
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

static void write_board_with_first_row(const char first_row[BOARD_SIZE + 1])
{
    FILE *file = fopen(temp_board_path, "w");
    assert(file != NULL);

    write_csv_row(file, first_row);

    for (int row_index = 1; row_index < BOARD_SIZE; ++row_index) {
        write_csv_row(file, "...............");
    }

    fclose(file);
}

static void write_text_file(const char *contents)
{
    FILE *file = fopen(temp_board_path, "w");
    assert(file != NULL);
    fputs(contents, file);
    fclose(file);
}

static uint16_t pack_first3(const char tiles[BOARD_SIZE + 1])
{
    uint16_t packed = UINT16_MAX;

    for (int col_index = 0; col_index < 3; ++col_index) {
        unsigned char tile = (unsigned char)tiles[col_index];

        if (isalpha(tile)) {
            uint16_t packed_tile = (uint16_t)(tile & 0x1F);
            int shift = (2 - col_index) * 5;
            packed = (uint16_t)((packed & ~(uint16_t)(0x1F << shift)) | (uint16_t)(packed_tile << shift));
        }
    }

    return packed;
}

static uint64_t pack_last12(const char tiles[BOARD_SIZE + 1])
{
    uint64_t packed = UINT64_MAX;

    for (int col_index = 3; col_index < BOARD_SIZE; ++col_index) {
        unsigned char tile = (unsigned char)tiles[col_index];

        if (isalpha(tile)) {
            uint64_t packed_tile = (uint64_t)(tile & 0x1F);
            int shift = (BOARD_SIZE - 1 - col_index) * 5;
            packed = (packed & ~((uint64_t)0x1F << shift)) | (packed_tile << shift);
        }
    }

    return packed;
}

static void assert_board_is_zeroed(Board board)
{
    for (int row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        assert(board.rows[row_index].first3Tiles == 0);
        assert(board.rows[row_index].last12Tiles == 0);
    }
}

static void loads_letters_from_csv_board(void)
{
    const char first_row[BOARD_SIZE + 1] = "ABCDEFGHIJKLMNO";

    write_board_with_first_row(first_row);

    Board board = board_from_csv(temp_board_path);

    assert(board.rows[0].first3Tiles == pack_first3(first_row));
    assert(board.rows[0].last12Tiles == pack_last12(first_row));

    for (int row_index = 1; row_index < BOARD_SIZE; ++row_index) {
        assert(board.rows[row_index].first3Tiles == UINT16_MAX);
        assert(board.rows[row_index].last12Tiles == UINT64_MAX);
    }
}

static void keeps_empty_tiles_as_blank_values(void)
{
    const char first_row[BOARD_SIZE + 1] = ".Z.A...........";

    write_board_with_first_row(first_row);

    Board board = board_from_csv(temp_board_path);

    assert(board.rows[0].first3Tiles == pack_first3(first_row));
    assert(board.rows[0].last12Tiles == pack_last12(first_row));
}

static void returns_zeroed_board_when_first_csv_row_is_malformed(void)
{
    write_text_file("A,B,C\n");

    Board board = board_from_csv(temp_board_path);

    assert_board_is_zeroed(board);
}

static void run_test(const char *name, void (*test)(void))
{
    test();
    cleanup_after_each();
    printf("PASS %s\n", name);
}

int main(void)
{
    setup_all();

    run_test("loads_letters_from_csv_board", loads_letters_from_csv_board);
    run_test("keeps_empty_tiles_as_blank_values", keeps_empty_tiles_as_blank_values);
    run_test("returns_zeroed_board_when_first_csv_row_is_malformed", returns_zeroed_board_when_first_csv_row_is_malformed);

    return 0;
}
