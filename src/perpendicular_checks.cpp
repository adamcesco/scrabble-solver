#include "perpendicular_checks.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {
constexpr uint32_t absent_prefix = 0x1FFFF;
constexpr uint32_t absent_suffix = 0x7FFF;
constexpr uint32_t ineligible = UINT32_MAX;
constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Locate the first tile in the contiguous prefix ending at last_tile.
uint16_t prefix_start_bit(uint16_t occupied, uint16_t last_tile)
{
    if ((occupied & last_tile) == 0) {
        return 0;
    }

    uint16_t z = (uint16_t)(~occupied & (last_tile - 1u));
    z |= z >> 1;
    z |= z >> 2;
    z |= z >> 4;
    z |= z >> 8;

    return (uint16_t)(z + 1u);
}

constexpr uint32_t packed_pair(uint32_t prefix, uint32_t suffix)
{
    return (prefix << 15) | suffix;
}

uint32_t assign_role(uint32_t &role, uint32_t &next_id)
{
    if (role == ineligible) {
        role = next_id++;
    }
    return role;
}
}

PerpendicularCheckTable::PerpendicularCheckTable(const WordTable &dictionary)
{
    roles_by_word.reserve(dictionary.count + alphabet.size() + 1);
    for (std::string_view word : dictionary.word_set) {
        roles_by_word.try_emplace(word);
    }
    for (size_t i = 0; i < alphabet.size(); ++i) {
        roles_by_word.try_emplace(alphabet.substr(i, 1));
    }
    roles_by_word.emplace("", RoleIds{absent_prefix, absent_suffix});

    uint32_t next_prefix = 0, next_suffix = 0;
    for (std::string_view word : dictionary.word_set) {
        if (word.size() < MIN_WORD_LEN || word.size() > BOARD_SIZE ||
            !std::all_of(word.begin(), word.end(), [](char c) { return c >= 'A' && c <= 'Z'; })) {
            continue;
        }
        for (size_t gap = 0; gap < word.size(); ++gap) {
            auto left = roles_by_word.find(word.substr(0, gap));
            auto right = roles_by_word.find(word.substr(gap + 1));
            if (left == roles_by_word.end() || right == roles_by_word.end()) {
                continue;
            }
            uint32_t prefix = assign_role(left->second.prefix, next_prefix);
            uint32_t suffix = assign_role(right->second.suffix, next_suffix);
            letters_by_pair[packed_pair(prefix, suffix)] |= 1u << (word[gap] - 'A');
        }
    }
}

PerpendicularCheckTable::RoleIds PerpendicularCheckTable::roles_for(std::string_view run) const
{
    auto found = roles_by_word.find(run);
    return found == roles_by_word.end() ? RoleIds{} : found->second;
}

uint32_t PerpendicularCheckTable::letters_for(uint32_t prefix, uint32_t suffix) const
{
    if (prefix == ineligible || suffix == ineligible) {
        return 0;
    }
    if (prefix == absent_prefix && suffix == absent_suffix) {
        return all_letters;
    }
    auto found = letters_by_pair.find(packed_pair(prefix, suffix));
    return found == letters_by_pair.end() ? 0 : found->second;
}

struct Board::RunStarts {
    const PerpendicularCheckTable &lookup;
    uint32_t prefixes[BOARD_SIZE][BOARD_SIZE] = {};
    uint32_t suffixes[BOARD_SIZE][BOARD_SIZE] = {};

    RunStarts(const Row (&rows)[BOARD_SIZE], const PerpendicularCheckTable &lookup);
};

Board::RunStarts::RunStarts(const Row (&rows)[BOARD_SIZE], const PerpendicularCheckTable &lookup)
    : lookup(lookup)
{
    for (int row = 0; row < BOARD_SIZE; ++row) {
        uint16_t occupied = rows[row].occupiedMask;
        for (int col = 0; col < BOARD_SIZE; ++col) {
            int start = col;
            char tiles[BOARD_SIZE];
            while (occupied & (1u << col)) {
                tiles[col - start] = char(rows[row].tiles >> row_tile_shift_at_col(col));
                ++col;
            }
            if (col > start) {
                auto roles = lookup.roles_for(std::string_view(tiles, col - start));
                prefixes[row][start] = roles.prefix;
                suffixes[row][start] = roles.suffix;
            }
        }
    }
}

void Board::prepare_perpendicular_checks(const PerpendicularCheckTable &lookup)
{
    auto normal = std::make_shared<RunStarts>(rows, lookup);
    perpendicular_runs = std::make_shared<RunStarts>(perpendicularRows, lookup);
    normal_runs = std::move(normal);
}

uint32_t Board::perpendicular_check_set(uint8_t row, uint8_t col) const
{
    if (!perpendicular_runs) {
        throw std::logic_error("Prepare perpendicular checks before querying the board");
    }
    uint16_t occupied = perpendicularRows[col].occupiedMask;
    uint16_t start = prefix_start_bit(occupied, (1u << row) >> 1);
    uint32_t prefix = start ? perpendicular_runs->prefixes[col][__builtin_ctz(start)] : absent_prefix;
    uint32_t suffix = (occupied & (1u << (row + 1))) ? perpendicular_runs->suffixes[col][row + 1] : absent_suffix;
    return perpendicular_runs->lookup.letters_for(prefix, suffix);
}
