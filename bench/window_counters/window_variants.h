#ifndef WINDOW_VARIANTS_H
#define WINDOW_VARIANTS_H

#include "benchmark_solver.h"

namespace benchmark {
enum class SubsetRejection { none, disjoint, capacity };

struct WindowSolve {
    DirectionalCounts counts;
    double solve_us;
};

struct WindowVariant {
    const char *name;
    WindowSolve (*solve)(const WordTable &, const WordPatternTable &, Board &, const Rack &);
};

// Resolve in the qualifier or worker before entering the typed solve driver.
const WindowVariant &window_variant(std::string_view name);
}
#endif
