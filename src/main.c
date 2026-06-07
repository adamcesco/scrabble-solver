#include "board.h"
#include "dictionary.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    const char *board_path = argc > 1 ? argv[1] : "data/boards/sample_board.csv";
    const char *dictionary_path = argc > 2 ? argv[2] : "data/dictionaries/sample_dictionary.txt";

    Board board = board_from_csv(board_path);
    WordTable dictionary = words_from_file(dictionary_path);

    board_print(board);
    printf("loaded %zu words from %s\n", dictionary.count, dictionary_path);

    word_table_destroy(&dictionary);

    return 0;
}
