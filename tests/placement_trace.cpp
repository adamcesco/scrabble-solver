#include "placement_trace.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

static std::string hex_value(RowTiles value)
{
    std::ostringstream out;
    out << "0x" << std::hex;
    if (auto high = static_cast<uint64_t>(value >> 64)) {
        out << high << std::setfill('0') << std::setw(16);
    }
    out << static_cast<uint64_t>(value);
    return out.str();
}

static void check_rows(const char *direction, const Row (&expected)[BOARD_SIZE], const Row (&actual)[BOARD_SIZE])
{
    for (int row = 0; row < BOARD_SIZE; ++row) {
        auto check = [&](const char *field, RowTiles before, RowTiles after) {
            if (before != after) {
                throw std::runtime_error(std::string(direction) + " row " + std::to_string(row) + " " + field +
                                         ": expected " + hex_value(before) + ", got " + hex_value(after));
            }
        };
        check("tiles", expected[row].tiles, actual[row].tiles);
        check("occupiedMask", expected[row].occupiedMask, actual[row].occupiedMask);
    }
}

void SolverBoardSnapshot::check(const Board &board) const
{
    check_rows("across", original.rows, board.rows);
    check_rows("down", original.perpendicularRows, board.perpendicularRows);
    // These pointers identify immutable grids, including IDs that no candidate reads.
    if (board.normal_runs != original.normal_runs) {
        throw std::runtime_error("across perpendicular-check grid changed");
    }
    if (board.perpendicular_runs != original.perpendicular_runs) {
        throw std::runtime_error("down perpendicular-check grid changed");
    }
}

bool SolverBoardSnapshot::is_down(const Board &board) const
{
    return board.normal_runs == original.perpendicular_runs;
}

namespace {
struct Recording {
    const WordTable &words;
    const Board &board;
    SolverBoardSnapshot snapshot;
    ViewDecisions decisions;
};

thread_local Recording *active;
}

void trace_placements(uint8_t row, uint8_t start, const uint32_t *ids, size_t size, bool accepted)
{
    if (!active) return;
    const bool down = active->snapshot.is_down(active->board);
    for (size_t i = 0; i < size; ++i) {
        active->decisions[down].emplace_back(down, row, start, active->words.words[ids[i]], accepted);
    }
}

SolveTrace trace_moves(const WordTable &words, const WordPatternTable &patterns, Board &board, SortedRack rack)
{
    Recording recording{words, board, {board}, {}};
    struct RestoreRecording {
        Recording *previous;
        ~RestoreRecording() { active = previous; }
    } restore{active};
    active = &recording;
    const auto counts = count_moves(words, patterns, board, rack.tiles, rack.length);
    recording.snapshot.check(board);
    for (bool down : {false, true}) {
        const auto &view = recording.decisions[down];
        const size_t accepted = std::count_if(view.begin(), view.end(),
            [](const Decision &d) { return std::get<4>(d); });
        if (accepted != (down ? counts.down : counts.across)) {
            throw std::runtime_error("directional count differs from accepted candidate decisions");
        }
    }
    return {counts, std::move(recording.decisions)};
}
