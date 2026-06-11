#include "board.h"
#include "dictionary.h"
#include "validation.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

int main(int argc, char **argv)
{
    const char *board_file_path = argc > 1 ? argv[1] : "data/boards/sample_board.csv";
    const char *dictionary_path = argc > 2 ? argv[2] : "data/dictionaries/sample_dictionary.txt";

    // load word-oriented tables
    WordTable dictionary = words_from_file(dictionary_path);
    WordStartRowTable starting_position_to_row = word_start_row_table_from_word_table(&dictionary);
    uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1];
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
    clock_t move_generate_start = clock();
    int count = 0;
    
    // these values placed outside all of the loops to avoid repeated destructions and initializations
    Row row_with_newly_added_word = {0};
    Board copy_of_original_board = board;
    // for each row in the board
    for (uint8_t row_index = 0; row_index < BOARD_SIZE; ++row_index) {
        // for each word in the dictionary
        for (size_t entry_index = 0; entry_index < starting_position_to_row.count; ++entry_index) {
            const WordStartRowEntry *entry = &starting_position_to_row.entries[entry_index];

            // for each possible placement of that word in a row on the board
            for (uint8_t start = 0; start < BOARD_SIZE; ++start) {
                if ((entry->valid_starts & (uint16_t)(UINT16_C(1) << start)) == 0) {
                    continue;
                }
                
                const Row *row_with_just_proposed_word = &entry->rows[start];

                if (is_placeable_on_row(&board.rows[row_index], row_with_just_proposed_word)) {
                    if (place_word_onto_perpendicular_rows_and_validate(
                        &dictionary,
                        config_to_start_positions,
                        copy_of_original_board.perpendicularRows,
                        &board,
                        row_with_just_proposed_word,
                        row_index,
                        start,
                        entry->word_length
                    )) {
                        ++count;
                        
                        add_proposed_word_to_row(&row_with_newly_added_word, &board.rows[row_index], row_with_just_proposed_word);
                        board.rows[row_index] = row_with_newly_added_word;
                        // now `board` is the new version of the board that has the new word placed within it

                        printf("row %2d ", row_index + 1);
                        print_row(board.rows[row_index]);

                        // restoring the board back to it's original state
                        board.rows[row_index] = copy_of_original_board.rows[row_index];
                    }

                    // restoring the board back to it's original state
                    memcpy(board.perpendicularRows, copy_of_original_board.perpendicularRows, sizeof(board.perpendicularRows));
                }
            }
        }
    }
    clock_t move_generate_end = clock();
    double move_generate_seconds = (double)(move_generate_end - move_generate_start) / CLOCKS_PER_SEC;
    
    printf("There are %2d possible moves", count);
    printf("\nMove generation took %.6f seconds", move_generate_seconds);
    printf("\n");

    // sanity print to insure that the original board was not modified
    board_print(board);
    printf("\n");

    // destroy hash-tables and hash-maps
    word_start_row_table_destroy(&starting_position_to_row);
    word_table_destroy(&dictionary);

    return 0;
}
