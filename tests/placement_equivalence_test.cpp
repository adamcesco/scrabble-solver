#include "placement_comparison.h"
#include "perpendicular_fixtures.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using WindowDecisions = std::vector<std::pair<std::string, bool>>;

static WindowDecisions middle_window(const Decisions &decisions)
{
    WindowDecisions result;
    for (const auto &[down, row, start, word, accepted] : decisions) {
        if (row == 1 && start == 0 && word.size() == 3) {
            result.emplace_back(word, accepted);
        }
    }
    return result;
}

static void compare_rt_orders(bool tar_first, const WindowVariants &variants)
{
    WordTable words = load_dictionary(tar_first ? "AS\nSAT\nTO\nAR\nAT\nTAR\nRAT\n"
                                          : "AS\nSAT\nTO\nAR\nAT\nRAT\nTAR\n");
    {
        PlacementComparison fixture{words};
        for (bool transposed : {false, true}) {
            Board board = board_at(0, 0, {"AS.", ".A.", ".TO"});
            if (transposed) {
                board.swap_views();
            }
            auto decisions = fixture.compare_board("RT", board, "RT", variants);
            board.prepare_perpendicular_checks(fixture.lookup);
            SolverBoardSnapshot snapshot{board};
            const benchmark::Rack rack("RT");
            auto rt = rack_subset_keys_for_rack(rack.tiles, rack.length);
            auto counts = rack_oriented_both_directions(&words, &fixture.patterns, &rt, &board, rack.length);
            snapshot.check(board, false);
            assert(counts.across == accepted_count(decisions[0]));
            assert(counts.down == accepted_count(decisions[1]));
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
            for (const auto *variant : variants) {
                // The second solve starts on the same board after the first restored its view.
                for (int repeat = 0; repeat < 2; ++repeat) {
                    PlacementTrace trace{words, board, snapshot};
                    auto actual = trace.solve(*variant, fixture.patterns, rack);
                    assert(actual.across == counts.across && actual.down == counts.down);
                    assert(middle_window(trace.decisions[transposed]) == expected);
                }
            }
            auto [across, down] = fixture.compare_board("unmatched rack", board, "ZZ", variants);
            assert(across.empty() && down.empty());
        }
    }
    words.destroy();
}

static void compare_rack_groups_and_ruled_out_windows(const WindowVariants &variants)
{
    WordTable words = load_dictionary("AS\nASX\nSAT\nTO\nAR\nAT\nTAR\nRAT\n");
    {
        PlacementComparison fixture{words};
        for (bool blocked : {false, true}) {
            Board board = board_at(0, 0, {blocked ? "ASX" : "AS.", ".A.", ".TO"});
            auto [across, down] = fixture.compare_board("rack groups", board, "RST", variants);
            // SAT uses ST; TAR and RAT use RT. The X.O crossing disallows all three.
            WindowDecisions actual = middle_window(across);
            std::sort(actual.begin(), actual.end());
            const WindowDecisions expected{{"RAT", !blocked}, {"SAT", !blocked}, {"TAR", false}};
            assert(actual == expected);
        }
    }
    words.destroy();
}

static void compare_duplicate_group_entries(const WindowVariants &variants)
{
    WordTable words = load_dictionary("ARS\nART\nATR\n");
    {
        PlacementComparison fixture{words};
        // Dictionary loading deduplicates spellings. Give A..'s RT group two copies of ART's ID.
        const auto *pattern = fixture.patterns.get({'A', 1, 3});
        assert(pattern);
        const auto *group = fixture.patterns.get_anagram(*pattern, benchmark::Rack("RT").tiles);
        assert(group && group->word_indices.count == 2);
        auto &ids = fixture.patterns.word_indices;
        const auto start = group->word_indices.start;
        ids[start + 1] = ids[start];
        for (bool impossible : {false, true}) {
            Board board = board_at(0, 0, {impossible ? "..X" : "...", "A.."});
            const auto decisions = fixture.compare_board("duplicate RT entries", board, "RST", variants);
            auto actual = middle_window(decisions[0]);
            std::sort(actual.begin(), actual.end());
            const WindowDecisions expected{{"ARS", !impossible}, {"ART", !impossible}, {"ART", !impossible}};
            assert(actual == expected);
        }
    }
    words.destroy();
}

static void one_tile_counts_in_both_directions_and_restores_the_prepared_board(const WindowVariants &variants)
{
    WordTable words = load_dictionary("AS\nST\n");
    {
        PlacementComparison fixture{words};
        Board board = board_at(0, 0, {"AS", "S."});
        fixture.compare_board("one tile", board, "T", variants);
        board.prepare_perpendicular_checks(fixture.lookup);
        SolverBoardSnapshot snapshot{board};
        const benchmark::Rack tiles("T");
        auto rack = rack_subset_keys_for_rack(tiles.tiles, tiles.length);
        const auto counts = rack_oriented_both_directions(&words, &fixture.patterns, &rack, &board, 1);
        assert(counts.across == 1 && counts.down == 1);
        snapshot.check(board, false);
    }
    words.destroy();
}

static void compare_gap_keys_at_the_board_edge(const WindowVariants &variants)
{
    WordTable words = load_dictionary("AS\nAT\nTA\nTO\nSO\nRAT\nTAR\n");
    {
        PlacementComparison fixture{words};
        Board edge = board_at(0, BOARD_SIZE - 2, {"AS", "T."});
        auto [across, down] = fixture.compare_board("last-column gap", edge, "AO", variants);
        // TA and TO fill the same window. Only O forms a perpendicular word with S.
        for (const auto &key : {Decision{false, 1, 13, "TA", false}, Decision{false, 1, 13, "TO", true}}) {
            assert(std::count(across.begin(), across.end(), key) == 1);
        }
        Board board = board_at(1, 0, {".A.", ".TO"});
        board.prepare_perpendicular_checks(fixture.lookup);
        assert(board.perpendicular_check_set(1, 0) == PerpendicularCheckTable::all_letters);
        assert(board.perpendicular_check_set(1, 2) != PerpendicularCheckTable::all_letters);
        auto decisions = fixture.compare_board("one constrained gap of two", board, "RT", variants);
        const WindowDecisions expected{{"RAT", true}, {"TAR", false}};
        assert(middle_window(decisions[0]) == expected);
    }
    words.destroy();
}

static void compare_each_gap_in_short_words(const WindowVariants &variants)
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
            PlacementComparison fixture{words};
            const int start = BOARD_SIZE - length;
            for (size_t gap = 0; gap < length; ++gap) {
                // Only QA is a word. Move the crossing through every SIMD lane and scalar tail.
                Board board = board_at(0, start + gap, {"Q"});
                auto decisions = fixture.compare_board("right-edge anagrams", board, letters, variants);
                for (const auto &word : candidates) {
                    const Decision expected{false, 1, start, word, word[gap] == 'A'};
                    assert(std::count(decisions[0].begin(), decisions[0].end(), expected) == 1);
                }
            }
        }
        words.destroy();
    }
}

static void reuse_check_sets_across_overlapping_windows()
{
    WordTable words = load_dictionary("AX\nAT\nCAT\nTO\n");
    {
        PlacementComparison fixture{words};
        Board board = board_at(0, 0, {"AX"});
        const auto &cached = benchmark::window_variant("row-cache");
        const auto &packed = benchmark::window_variant("packed");
        board.prepare_perpendicular_checks(fixture.lookup);
        const SolverBoardSnapshot snapshot{board};
        assert(board.perpendicular_check_set(1, 0) != 0); // Below A, T and X are allowed.
        assert(board.perpendicular_check_set(1, 1) == 0); // No word starts with X.

        struct RackCase {
            const char *letters;
            unsigned below_a, below_x;
            ViewDecisions expected;
        };
        // AT and CAT start at 0 and 1 below AX. The second rack matches only TO.
        RackCase racks[] = {{"ACT", 2, 4, {}}, {"OT", 1, 2, {}}};
        for (auto &rack : racks) {
            rack.expected = fixture.compare_board("overlapping windows below AX", board, rack.letters, {&cached});
            PlacementTrace baseline{words, board, snapshot};
            baseline.solve(packed, fixture.patterns, benchmark::Rack(rack.letters));
            assert(baseline.lookups[0][1][0] == rack.below_a);
            assert(baseline.lookups[0][1][1] == rack.below_x);
        }
        for (const auto &rack : racks) {
            PlacementTrace reuse{words, board, snapshot};
            reuse.solve(cached, fixture.patterns, benchmark::Rack(rack.letters));
            assert(reuse.decisions == rack.expected);
            assert(reuse.lookups[0][1][0] == 1 && reuse.lookups[0][1][1] == 1);
        }
    }
    words.destroy();
}

static std::filesystem::path qualifier_project(const char *executable)
{
    // Keep the invocation path so a symlink staged under build/ belongs to that project.
    auto dir = std::filesystem::absolute(executable).parent_path();
    for (; dir.has_relative_path(); dir = dir.parent_path()) {
        if (dir.filename() == "build") {
            return dir.parent_path();
        }
    }
    return {};
}

enum class CacheMode { discover, require_existing, build_if_missing };

// A replay can append the expected play: down (0/1), view row, start, complete word.
static void qualify_inputs(const char *dictionary_path, const std::filesystem::path &requested, const char *executable,
                           CacheMode mode, const WindowVariants &variants, const char *profile_path)
{
    WordTable words = WordTable::from_file(dictionary_path);
    assert(words.count > 0);
    {
        const auto [cache_path, existing] = mode == CacheMode::discover
            ? benchmark::select_pattern_cache(words, requested, qualifier_project(executable))
            : benchmark::PatternCache{requested, mode == CacheMode::require_existing || std::filesystem::exists(requested)};
        std::cerr << "Qualification " << (existing ? "loading " : "building ") << cache_path << '\n';
        PlacementComparison fixture{words, existing ? WordPatternTable::load(words, cache_path.c_str())
                                                    : WordPatternTable::from_words(words)};
        if (fixture.patterns.is_empty() || fixture.rows.count != words.count) {
            throw std::runtime_error("could not prepare qualification dictionary and pattern table");
        }
        if (!existing && !fixture.patterns.save(words, cache_path.c_str())) {
            throw std::runtime_error("could not save the qualification pattern cache");
        }

        std::ofstream visits;
        WorkloadOutput profile{visits};
        std::stringstream saved_requests;
        std::istream *requests = &std::cin;
        if (profile_path) {
            if (std::filesystem::exists(profile_path)) {
                // Check every input, including later boards, before truncating an existing CSV.
                auto protect = [&](const std::filesystem::path &path) {
                    if (std::filesystem::equivalent(profile_path, path)) {
                        throw std::invalid_argument("profile output aliases input: " + path.string());
                    }
                };
                protect(dictionary_path);
                protect(cache_path);
                for (std::string line; std::getline(std::cin, line);) {
                    protect(line.substr(0, line.find('\t')));
                    saved_requests << line << '\n';
                }
                requests = &saved_requests;
            }
            visits.exceptions(std::ios::failbit | std::ios::badbit);
            visits.open(profile_path);
            profile.write_header();
        }
        std::cout << "across,across_candidates,down,down_candidates\n";
        std::string line;
        while (std::getline(*requests, line)) {
            std::istringstream fields(line);
            std::string path, letters;
            std::getline(fields, path, '\t');
            std::getline(fields, letters, '\t');
            assert(!letters.empty() && letters.size() <= MAX_RACK_TILES);
            std::optional<Decision> required_play;
            if (!fields.eof()) {
                bool down;
                int row, start;
                std::string word;
                if (!(fields >> down >> row >> start >> word)) {
                    throw std::runtime_error("invalid expected play for " + path);
                }
                required_play.emplace(down, row, start, word, true);
            }
            Board original = board_from_csv(path.c_str());
            if (std::none_of(std::begin(original.rows), std::end(original.rows),
                             [](const Row &row) { return row.occupiedMask != 0; })) {
                throw std::runtime_error("unreadable or empty post-opening board: " + path);
            }
            auto [across, down] = fixture.compare_board(path.c_str(), original, letters, variants, required_play,
                                                       profile_path ? &profile : nullptr);
            std::cout << accepted_count(across) << ',' << across.size() << ','
                      << accepted_count(down) << ',' << down.size() << '\n';
            ++profile.position;
        }
        if (profile_path) {
            visits.close();
        }
    }
    words.destroy();
}

int main(int argc, char **argv)
{
    try {
        WindowVariants variants;
        CacheMode mode = CacheMode::discover;
        const std::string_view option = argc > 3 ? argv[3] : "";
        if (option == "--load-cache") {
            mode = CacheMode::require_existing;
        } else if (option == "--build-cache") {
            mode = CacheMode::build_if_missing;
        }
        int i = mode == CacheMode::discover ? 3 : 4;
        const char *profile_path = nullptr;
        if (i < argc && std::string_view(argv[i]) == "--profile") {
            if (++i == argc) {
                throw std::invalid_argument("--profile requires a CSV path");
            }
            profile_path = argv[i++];
        }
        for (; i < argc; ++i) {
            variants.push_back(&benchmark::window_variant(argv[i]));
        }
        if (variants.empty()) {
            variants.push_back(&benchmark::window_variant("packed"));
        }
        if (argc == 1) {
            constexpr const char *batch_and_simd_counters[] = {
                "batch-2", "batch-4", "batch-2-prefetch", "batch-4-prefetch",
#ifdef __clang__
                "batch-2-interleave", "batch-4-interleave",
#endif
#ifdef __AVX2__
                "bounded-simd",
#endif
            };
            for (const char *name : batch_and_simd_counters) {
                variants.push_back(&benchmark::window_variant(name));
            }
        }
        compare_gap_keys_at_the_board_edge(variants);
        compare_each_gap_in_short_words(variants);
        reuse_check_sets_across_overlapping_windows();
        compare_rt_orders(true, variants);
        compare_rt_orders(false, variants);
        compare_rack_groups_and_ruled_out_windows(variants);
        compare_duplicate_group_entries(variants);
        one_tile_counts_in_both_directions_and_restores_the_prepared_board(variants);
        if (argc >= 3) {
            qualify_inputs(argv[1], argv[2], argv[0], mode, variants, profile_path);
        } else if (argc != 1) {
            return 1;
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
