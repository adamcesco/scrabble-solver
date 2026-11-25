// One dictionary, one prepared position per pair, and the same warm-up for each counter.
#include "window_variants.h"

#include <array>
#include <cstdio>
#include <iostream>
#include <sstream>

#ifdef WINDOW_TUNING_HOST
#include "tuning_loader.h"
#endif

using benchmark::WindowVariant;

static void measure_pairs(const char *dictionary, const char *cache, const WindowVariant *variants[2])
{
    WordTable words = WordTable::from_file(dictionary);
    auto patterns = WordPatternTable::load(words, cache);
    if (!words.count || patterns.is_empty()) {
        throw std::runtime_error("could not load qualified dictionary and pattern table");
    }
    {
        PerpendicularCheckTable lookup(words);
        std::string line;
        while (std::getline(std::cin, line)) {
            std::istringstream fields(line);
#ifdef WINDOW_TUNING_HOST
            if (line.rfind("@pair\t", 0) == 0) {
                fields.ignore(sizeof("@pair\t") - 1);
                variants[0] = load_tuning_counter(fields);
                variants[1] = load_tuning_counter(fields);
                std::fflush(stdout);
                continue;
            }
            if (!variants[0] || !variants[1]) {
                throw std::invalid_argument("select both builds before sending a position");
            }
#endif
            std::string path, letters, first;
            if (!std::getline(fields, path, '\t') || !std::getline(fields, letters, '\t') ||
                !std::getline(fields, first) || (first != "0" && first != "1")) {
                throw std::invalid_argument("expected board, rack and first variant index");
            }
            const benchmark::Rack rack(letters);
            Board board = board_from_csv(path.c_str());
            board.prepare_perpendicular_checks(lookup);
            const int first_index = first == "1";
            const std::array<int, 2> order{first_index, 1 - first_index};
            benchmark::WindowSolve solves[2]{};
            constexpr int warmup_rounds = 2;
            for (int round = 0; round <= warmup_rounds; ++round) {
                for (int i : order) {
                    auto solve = variants[i]->solve(words, patterns, board, rack);
                    if (round && (solve.counts.across != solves[i].counts.across ||
                                  solve.counts.down != solves[i].counts.down)) {
                        throw std::runtime_error(path + ": directional counts changed between solves");
                    }
                    solves[i] = solve;
                }
            }
            for (int i : order) {
                std::printf("%s,%zu,%zu,%.3f\n", variants[i]->name, solves[i].counts.across,
                            solves[i].counts.down, solves[i].solve_us);
            }
#ifdef WINDOW_TUNING_HOST
            std::fflush(stdout);
#endif
        }
    }
    words.destroy();
}

int main(int argc, char **argv)
{
    try {
        if (auto result = benchmark::pattern_cache_command(argc, argv)) {
            return *result;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--build-info") {
            std::puts(__VERSION__);
            std::puts(WINDOW_BUILD_FLAGS);
            return 0;
        }
#ifdef WINDOW_TUNING_HOST
        if (argc == 3) {
            const WindowVariant *variants[2]{};
            measure_pairs(argv[1], argv[2], variants);
            return 0;
        }
#endif
        if (argc != 5) {
            std::fprintf(stderr, "usage: %s baseline challenger dictionary cache\n", argv[0]);
            return 1;
        }
        const WindowVariant *variants[] = {&benchmark::window_variant(argv[1]), &benchmark::window_variant(argv[2])};
        measure_pairs(argv[3], argv[4], variants);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
