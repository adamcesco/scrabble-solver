#include "board.h"
#include "dictionary.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    const char *board_file_path = argc > 1 ? argv[1] : "data/boards/sample_board.csv";
    const char *word_config_file_path = argc > 2 ? argv[2] : "data/boards/sample_board_config.csv";
    const char *dictionary_path = argc > 3 ? argv[3] : "data/dictionaries/sample_dictionary.txt";

    uint16_t index_to_config[MAX_NUMBER_OF_START_CONFIGS] = {0};
    uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE] = {0};
    init_config_maps(index_to_config, config_to_index);

    Board board = board_from_csv(board_file_path, word_config_file_path, config_to_index);
    WordTable dictionary = words_from_file(dictionary_path);

    board_print(board);
    board_print_words_configs(board);
    printf("loaded %zu words from %s\n", dictionary.count, dictionary_path);

    word_table_destroy(&dictionary);

    return 0;
}
