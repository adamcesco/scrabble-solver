#ifndef SRC_SOLVERS_PLACE_AND_RESTORE_COUNTER_H
#define SRC_SOLVERS_PLACE_AND_RESTORE_COUNTER_H

#include "../shared/rack_windows.h"
#include "validation_hot.h"

namespace rack_solver {
struct PlaceAndRestoreCounter {
    const WordTable &dictionary;
    const WordPatternTable &patterns;
    const WordIndexStartRowTable &word_start_rows;
    Board &board;
    const Board original_board;

    PlaceAndRestoreCounter(const WordTable &dictionary, const WordPatternTable &patterns,
                           const WordIndexStartRowTable &word_start_rows, Board &board)
        : dictionary(dictionary), patterns(patterns), word_start_rows(word_start_rows),
          board(board), original_board(board) {}

    template <typename VisitGroups>
    size_t operator()(const RowCheckSets &checks, uint16_t, const MatchedWindow &window,
                      uint8_t, VisitGroups visit_groups) const
    {
        size_t count = 0;
        visit_groups([&](const WordPatternAnagramGroup &group) {
            for (size_t offset = 0; offset < group.word_indices.count; ++offset) {
                const uint32_t &word_index = patterns.word_indices[group.word_indices.start + offset];
                const bool accepted = place_and_restore_word(word_index, checks.row, window);
                count += accepted;
                trace_placements(checks.row, window.start, &word_index, 1, accepted);
            }
            return GroupSearch::keep_going;
        });
        return count;
    }

private:
    bool place_and_restore_word(uint32_t word_index, uint8_t row, const MatchedWindow &window) const
    {
        const Row *word = word_start_rows.get_unchecked(word_index, window.start);
        uint16_t touched_columns = 0;
        const bool accepted = hot_place_word_onto_perpendicular_rows_and_validate_with_touched_mask(
            &dictionary, original_board.perpendicularRows, &board, word, row,
            window.start, window.length, &touched_columns);
        while (touched_columns != 0) {
            const uint8_t column = (uint8_t)__builtin_ctz(touched_columns);
            board.perpendicularRows[column] = original_board.perpendicularRows[column];
            touched_columns &= (uint16_t)(touched_columns - 1u);
        }
        return accepted;
    }
};
} // namespace rack_solver

#endif
