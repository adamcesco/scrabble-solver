// Keep calls inside each measured library local, including under full LTO.
// Only the registry lookup crosses the library boundary, before either clock read.
#include "window_variants.h"

extern "C" __attribute__((visibility("default")))
const benchmark::WindowVariant *scrabble_window_counter(const char *name)
{
    return &benchmark::window_variant(name);
}
