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

    // load board
    Board board = board_from_csv(board_file_path);

    // printing
    board_print(board);
    printf("\n");
    board_print_perpendicular(board);

    // printf("loaded %zu words from %s\n", dictionary.count, dictionary_path);
    // word_table_print(&dictionary);

    // printf("created %zu word start row entries\n", starting_position_to_row.count);
    // word_start_row_table_print(&starting_position_to_row);

    // destroy hash-tables and hash-maps
    word_start_row_table_destroy(&starting_position_to_row);
    word_table_destroy(&dictionary);

    return 0;
}
