#ifndef MEMORY_TRACE_H
#define MEMORY_TRACE_H

#include <cstddef>

namespace memory_trace {
extern bool enabled;
void scratch(size_t bytes);
}

#endif
