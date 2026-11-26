#include "placement_trace.h"
#include "perpendicular_checks.h"
#include "perpendicular_fixtures.h"
#include "selective_anagram_fixture.h"
#include "utils.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <utility>

struct PositionFixture {
    const WordTable &words;
    WordPatternTable patterns = WordPatternTable::from_words(words);
    PerpendicularCheckTable lookup{words};

    ViewDecisions decisions(Board board, const std::string &letters) const
    {
        board.prepare_perpendicular_checks(lookup);
        return trace_moves(words, patterns, board, SortedRack(letters)).decisions;
    }
};

using WindowDecisions = std::vector<std::pair<std::string, bool>>;

static WindowDecisions three_letter_window(const Decisions &decisions, int window_row)
{
    WindowDecisions result;
    for (const auto &[down, row, start, word, accepted] : decisions) {
        if (row == window_row && start == 0 && word.size() == 3) {
            result.emplace_back(word, accepted);
        }
    }
    return result;
}

static void compare_rt_orders(bool tar_first)
{
    WordTable words = load_dictionary(tar_first ? "AS\nSAT\nTO\nAR\nAT\nTAR\nRAT\n"
                                          : "AS\nSAT\nTO\nAR\nAT\nRAT\nTAR\n");
    {
        PositionFixture fixture{words};
        for (bool transposed : {false, true}) {
            Board board = board_at(0, 0, {"AS.", ".A.", ".TO"});
            if (transposed) {
                board.swap_views();
            }
            board.prepare_perpendicular_checks(fixture.lookup);
            const SortedRack rack("RT");
            if (transposed) {
                board.swap_views();
            }
            const uint32_t r = 1u << ('R' - 'A'), t = 1u << ('T' - 'A');
            assert((board.perpendicular_check_set(1, 0) & (r | t)) == (r | t));
            assert((board.perpendicular_check_set(1, 2) & (r | t)) == t);
            if (transposed) {
                board.swap_views();
            }
            WindowDecisions expected{{"TAR", false}, {"RAT", true}};
            if (!tar_first) {
                std::reverse(expected.begin(), expected.end());
            }
            for (int repeat = 0; repeat < 2; ++repeat) {
                const auto trace = trace_moves(words, fixture.patterns, board, rack);
                assert(three_letter_window(trace.decisions[transposed], 1) == expected);
            }
            auto [across, down] = fixture.decisions(board, "ZZ");
            assert(across.empty() && down.empty());
        }
    }
    words.destroy();
}

static void compare_rack_groups_and_ruled_out_windows()
{
    WordTable words = load_dictionary("AS\nASX\nSAT\nTO\nAR\nAT\nTAR\nRAT\n");
    {
        PositionFixture fixture{words};
        for (bool blocked : {false, true}) {
            Board board = board_at(0, 0, {blocked ? "ASX" : "AS.", ".A.", ".TO"});
            auto [across, down] = fixture.decisions(board, "RST");
            // SAT uses ST; TAR and RAT use RT. The X.O crossing disallows all three.
            WindowDecisions actual = three_letter_window(across, 1);
            std::sort(actual.begin(), actual.end());
            const WindowDecisions expected{{"RAT", !blocked}, {"SAT", !blocked}, {"TAR", false}};
            assert(actual == expected);
        }
    }
    words.destroy();
}

static void repeated_rack_letters_survive_cache_round_trip()
{
    WordTable words = load_dictionary("FLEE\nFEEL\nEX\n");
    {
        auto built = WordPatternTable::from_words(words);
        char cache[] = "/tmp/scrabble_repeated_rack_XXXXXX";
        int fd = mkstemp(cache);
        assert(fd >= 0);
        close(fd);
        assert(built.save(words, cache));
        auto loaded = WordPatternTable::load(words, cache);
        unlink(cache);
        assert(!loaded.is_empty());

        auto board = board_at(0, 0, {"F.", ".X"});
        PerpendicularCheckTable lookup(words);
        board.prepare_perpendicular_checks(lookup);
        for (const std::string letters : {"EEL", "AEL"}) {
            const SortedRack rack(letters);
            const auto before = trace_moves(words, built, board, rack);
            const auto after = trace_moves(words, loaded, board, rack);
            assert(before.counts.across == after.counts.across && before.counts.down == after.counts.down);
            assert(before.decisions == after.decisions);

            // FEEL forms EX next to the X in both directions. FLEE would form LX.
            for (bool down : {false, true}) {
                const auto &view = before.decisions[down];
                for (const Decision &decision : {Decision{down, 0, 0, "FEEL", true},
                                                 Decision{down, 0, 0, "FLEE", false}}) {
                    assert(std::count(view.begin(), view.end(), decision) == (letters == "EEL"));
                }
            }
        }
    }
    words.destroy();
}

static void compare_duplicate_group_entries()
{
    WordTable words = load_dictionary("ARS\nART\nATR\n");
    {
        PositionFixture fixture{words};
        // Dictionary loading deduplicates spellings. Give A..'s RT group two copies of ART's ID.
        const auto *pattern = fixture.patterns.get({'A', 1, 3});
        assert(pattern);
        const auto *group = fixture.patterns.get_anagram(*pattern, SortedRack("RT").tiles);
        assert(group && group->word_indices.count == 2);
        auto &ids = fixture.patterns.word_indices;
        const auto start = group->word_indices.start;
        ids[start + 1] = ids[start];
        for (bool impossible : {false, true}) {
            Board board = board_at(0, 0, {impossible ? "..X" : "...", "A.."});
            const auto decisions = fixture.decisions(board, "RST");
            auto actual = three_letter_window(decisions[0], 1);
            std::sort(actual.begin(), actual.end());
            const WindowDecisions expected{{"ARS", !impossible}, {"ART", !impossible}, {"ART", !impossible}};
            assert(actual == expected);
        }
    }
    words.destroy();
}

static void compare_selective_anagram_windows()
{
    auto words = load_dictionary(selective_anagram_dictionary().c_str());
    auto built = WordPatternTable::from_words(words);
    // Supply a repeated dictionary ID, including in the v3 payload.
    const auto *pattern = built.get({put_byte_128('A', 0, BOARD_SIZE), 1, 3});
    assert(pattern);
    const auto *bc = built.get_anagram(*pattern, SortedRack("BC").tiles);
    assert(bc && bc->word_indices.count == 2);
    built.word_indices[bc->word_indices.start + 1] = built.word_indices[bc->word_indices.start];

    char path[] = "/tmp/scrabble_selective_anagrams_XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0 && close(fd) == 0);
    assert(built.save(words, path));
    auto loaded = WordPatternTable::load(words, path);
    unlink(path);
    assert(!loaded.is_empty());

    for (auto *table : {&built, &loaded}) {
        PositionFixture fixture{words, std::move(*table)};
        for (bool down : {false, true}) {
            auto board = board_at(0, 0, {"..X", "A..", "...", "A.."});
            if (down) board.swap_views();
            const auto decisions = fixture.decisions(board, "AABCI");
            // X above the final gap rules out the first A.. window. The next accepts
            // every matching ID, including two copies of ACB and the displaced BI key.
            for (int row : {1, 3}) {
                auto actual = three_letter_window(decisions[down], row);
                std::sort(actual.begin(), actual.end());
                const bool accepted = row == 3;
                const WindowDecisions expected{{"AAA", accepted}, {"AAB", accepted}, {"AAC", accepted},
                                               {"AAI", accepted}, {"ABI", accepted}, {"ACB", accepted},
                                               {"ACB", accepted}, {"ACI", accepted}};
                assert(actual == expected);
            }
        }
        auto miss = fixture.decisions(board_at(0, 0, {"A.."}), "GZ");
        assert(three_letter_window(miss[0], 0).empty());

        // AA. has 25 groups and two fixed tiles. One subset uses binary search,
        // seven distinct single-tile subsets use the sorted merge.
        for (const std::string rack : {"B", "ABCDEFG"}) {
            auto decisions = fixture.decisions(board_at(0, 0, {"AA."}), rack);
            auto actual = three_letter_window(decisions[0], 0);
            std::sort(actual.begin(), actual.end());
            WindowDecisions expected;
            for (char letter : rack) {
                expected.emplace_back(std::string("AA") + letter, true);
            }
            assert(actual == expected);
        }
    }
    words.destroy();
}

static void one_tile_counts_in_both_directions_and_restores_the_board()
{
    WordTable words = load_dictionary("AS\nST\n");
    {
        PositionFixture fixture{words};
        Board board = board_at(0, 1, {"AS", "S."});
        board.prepare_perpendicular_checks(fixture.lookup);
        for (bool transposed : {false, true}) {
            if (transposed) board.swap_views();
            auto result = trace_moves(words, fixture.patterns, board, SortedRack("T"));
            assert(result.counts.across == 1 && result.counts.down == 1);
        }
    }
    words.destroy();
}

static void compare_gap_keys_at_the_board_edge()
{
    WordTable words = load_dictionary("AS\nAT\nTA\nTO\nSO\nRAT\nTAR\n");
    {
        PositionFixture fixture{words};
        Board edge = board_at(0, BOARD_SIZE - 2, {"AS", "T."});
        auto [across, down] = fixture.decisions(edge, "AO");
        // TA and TO fill the same window. Only O forms a perpendicular word with S.
        for (const auto &key : {Decision{false, 1, 13, "TA", false}, Decision{false, 1, 13, "TO", true}}) {
            assert(std::count(across.begin(), across.end(), key) == 1);
        }
        Board board = board_at(1, 0, {".A.", ".TO"});
        board.prepare_perpendicular_checks(fixture.lookup);
        assert(board.perpendicular_check_set(1, 0) == PerpendicularCheckTable::all_letters);
        assert(board.perpendicular_check_set(1, 2) != PerpendicularCheckTable::all_letters);
        auto decisions = fixture.decisions(board, "RT");
        const WindowDecisions expected{{"RAT", true}, {"TAR", false}};
        assert(three_letter_window(decisions[0], 1) == expected);
    }
    words.destroy();
}

static void compare_each_gap_in_short_words()
{
    for (size_t length = 2; length <= MAX_RACK_TILES; ++length) {
        std::string letters(length, 'A');
        letters.back() = 'Z';
        std::string dictionary = "QA\n";
        std::vector<std::string> candidates;
        do {
            candidates.push_back(letters);
            dictionary += letters + "\n";
        } while (std::next_permutation(letters.begin(), letters.end()));
        WordTable words = load_dictionary(dictionary.c_str());
        {
            PositionFixture fixture{words};
            const int start = BOARD_SIZE - length;
            for (size_t gap = 0; gap < length; ++gap) {
                // Only QA is a word. Check a crossing at each possible offset.
                Board board = board_at(0, start + gap, {"Q"});
                auto decisions = fixture.decisions(board, letters);
                for (const auto &word : candidates) {
                    const Decision expected{false, 1, start, word, word[gap] == 'A'};
                    assert(std::count(decisions[0].begin(), decisions[0].end(), expected) == 1);
                }
            }
        }
        words.destroy();
    }
}

static void report_damaged_perpendicular_board()
{
    Board board{};
    SolverBoardSnapshot snapshot{board};
    board.perpendicularRows[7].occupiedMask = 4;
    try {
        snapshot.check(board);
    } catch (const std::runtime_error &error) {
        assert(std::string(error.what()).find("down row 7 occupiedMask") != std::string::npos);
        return;
    }
    throw std::runtime_error("board restoration check missed a changed occupied mask");
}

int main()
{
    report_damaged_perpendicular_board();
    compare_gap_keys_at_the_board_edge();
    compare_each_gap_in_short_words();
    compare_rt_orders(true);
    compare_rt_orders(false);
    compare_rack_groups_and_ruled_out_windows();
    repeated_rack_letters_survive_cache_round_trip();
    compare_duplicate_group_entries();
    compare_selective_anagram_windows();
    one_tile_counts_in_both_directions_and_restores_the_board();
}
