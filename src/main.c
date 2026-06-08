#include "board.h"
#include "dictionary.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    const char *board_file_path = argc > 1 ? argv[1] : "data/boards/sample_board.csv";
    const char *dictionary_path = argc > 2 ? argv[2] : "data/dictionaries/sample_dictionary.txt";

    // load word-oriented tables
    WordTable dictionary = words_from_file(dictionary_path);
    WordStartRowTable starting_position_to_row = word_start_row_table_from_word_table(&dictionary);
    uint16_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1];
    init_config_map(config_to_start_positions);

    // load board
    Board board = board_from_csv(board_file_path);

    // printing
    board_print(board);
    printf("\n");
    board_print_perpendicular(board);
    printf("\n");

    // printf("loaded %zu words from %s\n", dictionary.count, dictionary_path);
    // word_table_print(&dictionary);

    // printf("created %zu word start row entries\n", starting_position_to_row.count);
    // word_start_row_table_print(&starting_position_to_row);

    // test add a word
    int row_index = 8;
    for (size_t entry_index = 0; entry_index < starting_position_to_row.count; ++entry_index) {
        const WordStartRowEntry *entry = &starting_position_to_row.entries[entry_index];

        for (size_t start = 0; start < BOARD_SIZE; ++start) {
            if ((entry->valid_starts & (uint16_t)(UINT16_C(1) << start)) == 0) {
                continue;
            }
            
            const Row *row_with_just_proposed_word = &entry->rows[start];
            
            if (is_placeable_on_row(board.rows[row_index], *row_with_just_proposed_word)) {
                Board new_board = place_word_row_on_board(
                    board,
                    add_proposed_word_to_row(board.rows[row_index], *row_with_just_proposed_word),
                    row_index,
                    start,
                    entry->word_length);
                if (validate_perpendicular_rows(&dictionary, config_to_start_positions, new_board, start, entry->word_length)) {
                    printf("row %2d ", row_index + 1);
                    print_row(new_board.rows[row_index]);
                }
            }
        }
    }

    // destroy hash-tables and hash-maps
    word_start_row_table_destroy(&starting_position_to_row);
    word_table_destroy(&dictionary);

    return 0;
}
