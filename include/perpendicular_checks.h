#ifndef PERPENDICULAR_CHECKS_H
#define PERPENDICULAR_CHECKS_H

#include "board.h"
#include "word_table.h"

#include <string_view>
#include <unordered_map>

class PerpendicularCheckTable {
public:
    // The loaded dictionary owns the word storage and must outlive this table.
    explicit PerpendicularCheckTable(const WordTable &dictionary);
    PerpendicularCheckTable(WordTable &&) = delete;

private:
    friend struct Board;

    struct RoleIds {
        uint32_t prefix = UINT32_MAX;
        uint32_t suffix = UINT32_MAX;
    };
    RoleIds roles_for(std::string_view run) const;
    uint32_t letters_for(uint32_t prefix, uint32_t suffix) const;
    std::unordered_map<std::string_view, RoleIds> roles_by_word;
    std::unordered_map<uint32_t, uint32_t> letters_by_pair;
};

#endif
