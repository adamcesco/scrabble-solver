#include "dictionary_oriented_solver.h"

#include "validation.h"

#include <string.h>

int dictionary_oriented_solver(
    const WordTable *dictionary,
    const WordStartRowTable *starting_position_to_row,
    const uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1],
    Board *board
)
{
    int count = 0;

    // these values placed outside all of the loops to avoid repeated destructions and initializations
    Row row_with_newly_added_word = {};
    Board copy_of_original_board = *board;
    // for each row in the board
    // for each word in the dictionary
    for (size_t entry_index = 0; entry_index < starting_position_to_row->count; ++entry_index) {
        const WordStartRowEntry *entry = &starting_position_to_row->entries[entry_index];

        // for each possible placement of that word in a row on the board
        for (uint8_t start = 0; start < BOARD_SIZE; ++start) {
            if ((entry->valid_starts & (uint16_t)(UINT16_C(1) << start)) == 0) {
                break; // this break means that the rows in a WordStartRowEntry need to slide down the row in the 0 to 15 direction
            }
            const Row *row_with_just_proposed_word = &entry->rows[start];

            for (uint8_t row_index = 0; row_index < BOARD_SIZE; ++row_index) {
                if (is_placeable_on_row(&board->rows[row_index], row_with_just_proposed_word)) {
                    if (place_word_onto_perpendicular_rows_and_validate(
                        dictionary,
                        config_to_start_positions,
                        copy_of_original_board.perpendicularRows,
                        board,
                        row_with_just_proposed_word,
                        row_index,
                        start,
                        entry->word_length
                    )) {
                        ++count;

                        add_proposed_word_to_row(&row_with_newly_added_word, &board->rows[row_index], row_with_just_proposed_word);
                        board->rows[row_index] = row_with_newly_added_word;
                        // now `board` is the new version of the board that has the new word placed within it

                        // printf("row %2d ", row_index + 1);
                        // print_row(board.rows[row_index]);

                        // restoring the board back to it's original state
                        board->rows[row_index] = copy_of_original_board.rows[row_index];
                    }

                    // restoring the board back to it's original state
                    memcpy(board->perpendicularRows, copy_of_original_board.perpendicularRows, sizeof(board->perpendicularRows));
                }
            }
        }
    }

    return count;
}
