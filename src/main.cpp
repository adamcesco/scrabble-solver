#include "board.h"
#include "perpendicular_checks.h"
#include "solver.h"

#include <stdio.h>
#include <time.h>
#include <cstring>

static void solve_board(const WordTable &dictionary, const char *board_file_path)
{
    PerpendicularCheckTable lookup(dictionary);
    WordPatternTable pattern_and_rack_anagram_to_words = WordPatternTable::load(dictionary, "data/caches/pattern_and_rack_anagram_to_words.cache");
    if (pattern_and_rack_anagram_to_words.is_empty()) {
        pattern_and_rack_anagram_to_words = WordPatternTable::from_words(dictionary);
        (void)pattern_and_rack_anagram_to_words.save(dictionary, "data/caches/pattern_and_rack_anagram_to_words.cache");
    }
    
    // load board and rack
    Board board = board_from_csv(board_file_path);
    board.prepare_perpendicular_checks(lookup);

    const char * rack_cstring = "ABFHIKU";
    uint64_t rack = 0;
    uint8_t rack_length = (uint8_t)strlen(rack_cstring);
    memcpy(&rack, rack_cstring, rack_length);

    // printing
    board_print(board);
    printf("\n");

    // solving
    clock_t solve_start = clock();
    DirectionalCounts counts = count_moves(
        dictionary, pattern_and_rack_anagram_to_words, board, rack, rack_length
    );
    clock_t solve_end = clock();
    double solve_seconds = (double)(solve_end - solve_start) / CLOCKS_PER_SEC;
    
    // statistics
    printf("Moves: %zu\nSeconds: %.6f\n\n", counts.across + counts.down, solve_seconds);

    // sanity print to insure that the original board was not modified
    board_print(board);
    printf("\n");
}

int main(int argc, char **argv)
{
    const char *board_file_path = argc > 1 ? argv[1] : "data/boards/sample_board.csv";
    const char *dictionary_path = argc > 2 ? argv[2] : "data/dictionaries/sample_dictionary.txt";

    WordTable dictionary = WordTable::from_file(dictionary_path);
    solve_board(dictionary, board_file_path);
    dictionary.destroy();

    return 0;
}
