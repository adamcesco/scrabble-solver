#include "board.h"
#include "dictionary.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    const char *board_file_path = argc > 1 ? argv[1] : "data/boards/sample_board.csv";
    const char *dictionary_path = argc > 2 ? argv[2] : "data/dictionaries/sample_dictionary.txt";

    // load word-oriented tables
    WordTable dictionary = words_from_file(dictionary_path);
    WordStartRowTable word_start_rows = word_start_row_table_from_word_table(&dictionary);

    // load board
    Board board = board_from_csv(board_file_path);

    // printing
    board_print(board);

    printf("loaded %zu words from %s\n", dictionary.count, dictionary_path);
    word_table_print(&dictionary);

    printf("created %zu word start row entries\n", word_start_rows.count);
    word_start_row_table_print(&word_start_rows);

    // destroy hash-tables and hash-maps
    word_start_row_table_destroy(&word_start_rows);
    word_table_destroy(&dictionary);

    return 0;
}
