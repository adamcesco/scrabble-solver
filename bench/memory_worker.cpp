// One fresh process constructs one validator and solves the first board in both views.
#include "benchmark_solver.h"
#include "memory_trace.h"

#include <cstdio>

namespace {
size_t peak_resident_bytes()
{
    // VmHWM belongs to this executable's address space, including on small fixtures
    // where the Python parent had a larger resident set before exec.
    FILE *status = std::fopen("/proc/self/status", "r");
    if (!status) {
        throw std::runtime_error("cannot open /proc/self/status");
    }
    char line[256];
    size_t kib = 0;
    while (std::fgets(line, sizeof line, status)) {
        if (std::sscanf(line, "VmHWM: %zu kB", &kib) == 1) {
            break;
        }
    }
    std::fclose(status);
    if (!kib) {
        throw std::runtime_error("missing VmHWM");
    }
    return kib * 1024;
}

void measure(bool packed, bool expect_hit, const char *dictionary, const char *cache,
             const char *position, const benchmark::Rack &rack)
{
    memory_trace::enabled = true;
    WordTable words = WordTable::from_file(dictionary);
    if (!words.count) {
        throw std::runtime_error("dictionary read failed");
    }
    WordPatternTable patterns = WordPatternTable::load(words, cache);
    bool hit = !patterns.is_empty();
    if (hit != expect_hit) {
        throw std::runtime_error("pattern cache does not match expected starting condition");
    }
    if (!hit) {
        patterns = WordPatternTable::from_words(words);
        if (patterns.is_empty() || !patterns.save(words, cache)) {
            throw std::runtime_error("pattern cache build or save failed");
        }
    }
    std::printf("shared %zu\n", sizeof words + sizeof patterns);
    {
        benchmark::Validator validator(words, patterns, packed);
        size_t dictionary_rss = peak_resident_bytes();
        size_t lookup_size = packed ? sizeof(*validator.packed) : sizeof(validator.original);
        std::printf("validator %zu %zu\n", lookup_size, dictionary_rss);

        Board board = board_from_csv(position);
        if (std::none_of(std::begin(board.rows), std::end(board.rows),
                         [](const Row &row) { return row.occupiedMask != 0; })) {
            throw std::runtime_error("expected a readable post-opening board");
        }
        validator.prepare(board);
        std::printf("board %zu\n", sizeof board);
        memory_trace::enabled = false;

        auto counts = validator.solve(board, rack);
        size_t solve_rss = peak_resident_bytes();
        std::printf("solved %s %zu %zu %zu\n", hit ? "hit" : "miss",
                    counts.across, counts.down, solve_rss);
    }
    words.destroy();
}
}

int main(int argc, char **argv)
{
    if (argc == 4 && std::strcmp(argv[1], "--check-cache") == 0) {
        return benchmark::check_cache(argv[2], argv[3]);
    }
    if (argc != 7 || (std::strcmp(argv[1], "original") && std::strcmp(argv[1], "packed")) ||
        (std::strcmp(argv[2], "hit") && std::strcmp(argv[2], "miss"))) {
        std::fprintf(stderr, "usage: %s original|packed hit|miss dictionary cache board rack\n", argv[0]);
        return 1;
    }
    try {
        measure(std::strcmp(argv[1], "packed") == 0, std::strcmp(argv[2], "hit") == 0,
                argv[3], argv[4], argv[5], benchmark::Rack(argv[6]));
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
