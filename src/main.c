#include "board.h"

int main(int argc, char **argv)
{
    const char *board_path = argc > 1 ? argv[1] : "data/boards/sample_board.csv";

    Board board = board_from_csv(board_path);
    board_print(board);

    return 0;
}
