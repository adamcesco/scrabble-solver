#ifndef ROW_WINDOWS_H
#define ROW_WINDOWS_H

#include "board.h"
#include "utils.h"

#include <array>

inline constexpr auto row_window_tile_masks = [] {
    std::array<RowTiles, BOARD_SIZE + 1> masks{};
    for (int length = 1; length <= BOARD_SIZE; ++length) {
        masks[length] = (RowTiles{1} << (length * ROW_TILE_BITS)) - 1;
    }
    return masks;
}();

// Pass board.rows or board.perpendicularRows, a row in that view, and a limit in [0, 7].
// Calls visit(start_column, pattern, empty_count) once per connected, complete window.
// Returns the adjacent rows' occupancy union for perpendicular checks in this row.
template <typename Visitor>
uint16_t visit_row_windows(const Row (&rows)[BOARD_SIZE], uint8_t row_index, uint8_t empty_limit, Visitor &&visit)
{
    const Row &row = rows[row_index];
    const uint16_t neighbors = (row_index > 0 ? rows[row_index - 1].occupiedMask : 0)
        | (row_index + 1 < BOARD_SIZE ? rows[row_index + 1].occupiedMask : 0);
    for (uint8_t start = 0; start < BOARD_SIZE; ++start) {
        if (start > 0 && (row.occupiedMask & bit_at_u16(start - 1))) {
            continue;
        }
        uint8_t empty_count = 0;
        for (uint8_t end = start; end < BOARD_SIZE; ++end) {
            empty_count += (row.occupiedMask & bit_at_u16(end)) == 0;
            if (empty_count > empty_limit) {
                break;
            }
            if (end + 1 < BOARD_SIZE && (row.occupiedMask & bit_at_u16(end + 1))) {
                continue;
            }
            const uint8_t length = end - start + 1;
            if (length < MIN_WORD_LEN || empty_count == 0) {
                continue;
            }
            if (empty_count == length && (neighbors & bit_span_mask_u16(start, end)) == 0) {
                continue;
            }

            const PatternRow pattern{
                (row.tiles >> row_tile_shift_at_col(start)) & row_window_tile_masks[length],
                (uint16_t)((row.occupiedMask >> start) & low_bit_mask_u16(length)),
                length,
            };
            visit(start, pattern, empty_count);
        }
    }
    return neighbors;
}

#endif
