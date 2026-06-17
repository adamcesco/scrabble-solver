#include "board.h"
#include "dictionary.h"
#include "rack_oriented_solver.h"

#include <stdio.h>
#include <time.h>
#include <utility>

int main(int argc, char **argv)
{
    const char *board_file_path = argc > 1 ? argv[1] : "data/boards/sample_board.csv";
    const char *dictionary_path = argc > 2 ? argv[2] : "data/dictionaries/sample_dictionary.txt";

    // load word-oriented tables
    WordTable dictionary = WordTable::from_file(dictionary_path);
    
    WordIndexStartRowTable indexed_starting_position_to_row = WordIndexStartRowTable::from_words(dictionary);
    
    WordPatternTable pattern_and_rack_anagram_to_words = WordPatternTable::load(dictionary, "data/caches/pattern_and_rack_anagram_to_words.cache");
    if (pattern_and_rack_anagram_to_words.is_empty()) {
        pattern_and_rack_anagram_to_words = WordPatternTable::from_words(dictionary);
        (void)pattern_and_rack_anagram_to_words.save(dictionary, "data/caches/pattern_and_rack_anagram_to_words.cache");
    }
    
    uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1];
    init_config_map(config_to_start_positions);

    // load board
    Board board = board_from_csv(board_file_path);

    // printing
    board_print(board);
    printf("\n");

    // solving
    clock_t rack_oriented_start = clock();
    size_t horizontal_rack_oriented_count = rack_oriented_solver(
        &dictionary,
        &pattern_and_rack_anagram_to_words,
        &indexed_starting_position_to_row,
        config_to_start_positions,
        &board,
        "ABFHIKU"
    );
    std::swap(board.rows, board.perpendicularRows);
    size_t vertical_rack_oriented_count = rack_oriented_solver(
        &dictionary,
        &pattern_and_rack_anagram_to_words,
        &indexed_starting_position_to_row,
        config_to_start_positions,
        &board,
        "ABFHIKU"
    );
    std::swap(board.rows, board.perpendicularRows);
    clock_t rack_oriented_end = clock();
    double rack_oriented_seconds = (double)(rack_oriented_end - rack_oriented_start) / CLOCKS_PER_SEC;
    
    // statistics
    printf("Solver statistics\n");
    printf("-----------------\n");
    printf("%-28s %10s %14s\n", "Solver", "Moves", "Seconds");
    printf("%-28s %10zu %14.6f\n", "Rack oriented", (horizontal_rack_oriented_count + vertical_rack_oriented_count), rack_oriented_seconds);
    printf("\n");

    // sanity print to insure that the original board was not modified
    board_print(board);
    printf("\n");

    // destroy hash-tables and hash-maps
    indexed_starting_position_to_row.destroy();
    dictionary.destroy();

    return 0;
}
