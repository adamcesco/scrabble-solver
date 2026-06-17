#include "board.h"
#include "dictionary.h"
#include "rack_oriented_solver.h"

#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <utility>

int main(int argc, char **argv)
{
    const char *board_file_path = argc > 1 ? argv[1] : "data/boards/sample_board.csv";
    const char *dictionary_path = argc > 2 ? argv[2] : "data/dictionaries/sample_dictionary.txt";
    int iterations = argc > 3 ? atoi(argv[3]) : 5000;

    WordTable dictionary = WordTable::from_file(dictionary_path);
    WordIndexStartRowTable indexed_starting_position_to_row = WordIndexStartRowTable::from_words(dictionary);

    WordPatternTable pattern_and_rack_anagram_to_words =
        WordPatternTable::load(dictionary, "data/caches/pattern_and_rack_anagram_to_words.cache");
    if (pattern_and_rack_anagram_to_words.is_empty()) {
        pattern_and_rack_anagram_to_words = WordPatternTable::from_words(dictionary);
        (void)pattern_and_rack_anagram_to_words.save(dictionary, "data/caches/pattern_and_rack_anagram_to_words.cache");
    }

    uint8_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1];
    init_config_map(config_to_start_positions);

    Board board = board_from_csv(board_file_path);
    size_t total_moves = 0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        total_moves += rack_oriented_solver(
            &dictionary,
            &pattern_and_rack_anagram_to_words,
            &indexed_starting_position_to_row,
            config_to_start_positions,
            &board,
            "ABFHIKU"
        );
        std::swap(board.rows, board.perpendicularRows);
        total_moves += rack_oriented_solver(
            &dictionary,
            &pattern_and_rack_anagram_to_words,
            &indexed_starting_position_to_row,
            config_to_start_positions,
            &board,
            "ABFHIKU"
        );
        std::swap(board.rows, board.perpendicularRows);
    }
    auto end = std::chrono::steady_clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();
    printf("iterations %d\n", iterations);
    printf("total_moves %zu\n", total_moves);
    printf("seconds %.6f\n", seconds);
    printf("seconds_per_iteration %.9f\n", seconds / iterations);

    indexed_starting_position_to_row.destroy();
    dictionary.destroy();

    return 0;
}
