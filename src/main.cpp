#include "board.h"
#include "dictionary.h"
#include "dictionary_oriented_solver.h"
#include "rack_oriented_solver.h"

#include <stdio.h>
#include <time.h>

int main(int argc, char **argv)
{
    const char *board_file_path = argc > 1 ? argv[1] : "data/boards/sample_board.csv";
    const char *dictionary_path = argc > 2 ? argv[2] : "data/dictionaries/sample_dictionary.txt";

    // load word-oriented tables
    WordTable dictionary = words_from_file(dictionary_path);
    
    WordStartRowTable starting_position_to_row = word_start_row_table_from_word_table(&dictionary);
    
    WordPatternTable pattern_and_rack_anagram_to_words = word_pattern_table_load(&dictionary, "data/caches/pattern_and_rack_anagram_to_words.cache");
    if (word_pattern_table_is_empty(&pattern_and_rack_anagram_to_words)) {
        pattern_and_rack_anagram_to_words = word_pattern_table_from_word_table(&dictionary);
        (void)word_pattern_table_save(&pattern_and_rack_anagram_to_words, &dictionary, "data/caches/pattern_and_rack_anagram_to_words.cache");
    }
    printf("Largest word-pattern binary search domain: %zu\n", word_pattern_table_largest_binary_search_domain(&pattern_and_rack_anagram_to_words));
    
    uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1];
    init_config_map(config_to_start_positions);

    // load board
    Board board = board_from_csv(board_file_path);

    // printing
    // board_print(board);
    // printf("\n");
    // board_print_perpendicular(board);
    // printf("\n");
    // word_pattern_table_print(&pattern_and_rack_anagram_to_words, &dictionary);
    // printf("\n");

    // printf("loaded %zu words from %s\n", dictionary.count, dictionary_path);
    // word_table_print(&dictionary);

    // printf("created %zu word start row entries\n", starting_position_to_row.count);
    // word_start_row_table_print(&starting_position_to_row);

    clock_t dictionary_oriented_start = clock();
    int dictionary_oriented_count = dictionary_oriented_solver(
        &dictionary,
        &starting_position_to_row,
        config_to_start_positions,
        &board
    );
    clock_t dictionary_oriented_end = clock();
    double dictionary_oriented_seconds = (double)(dictionary_oriented_end - dictionary_oriented_start) / CLOCKS_PER_SEC;

    clock_t rack_oriented_start = clock();
    int rack_oriented_count = rack_oriented_solver(
        &dictionary,
        &pattern_and_rack_anagram_to_words,
        "",
        config_to_start_positions,
        &board
    );
    clock_t rack_oriented_end = clock();
    double rack_oriented_seconds = (double)(rack_oriented_end - rack_oriented_start) / CLOCKS_PER_SEC;
    
    printf("Solver statistics\n");
    printf("-----------------\n");
    printf("%-28s %10s %14s\n", "Solver", "Moves", "Seconds");
    printf("%-28s %10d %14.6f\n", "Dictionary oriented", dictionary_oriented_count, dictionary_oriented_seconds);
    printf("%-28s %10d %14.6f\n", "Rack oriented", rack_oriented_count, rack_oriented_seconds);
    printf("\n");

    // sanity print to insure that the original board was not modified
    board_print(board);
    printf("\n");

    // destroy hash-tables and hash-maps
    word_start_row_table_destroy(&starting_position_to_row);
    word_table_destroy(&dictionary);

    return 0;
}
