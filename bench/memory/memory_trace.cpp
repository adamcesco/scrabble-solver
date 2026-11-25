// GNU ld wraps allocation calls made by the worker and solver object files.
// Forward to the normal allocator. The reader owns all per-allocation bookkeeping.
#include "memory_trace.h"

#include <cstdio>

namespace memory_trace {
bool enabled = false;
static size_t reported_scratch = 0;

// For an unchanged set of live allocations only the largest overlapping
// scratch observation can change the peak.
void scratch(size_t bytes)
{
    if (enabled && bytes > reported_scratch) {
        std::printf("scratch %zu\n", bytes);
        reported_scratch = bytes;
    }
}

void acquire(void *address, size_t bytes)
{
    if (enabled && address) {
        reported_scratch = 0;
        std::printf("+ %p %zu\n", address, bytes);
    }
}

void release(void *address)
{
    if (enabled && address) {
        reported_scratch = 0;
        std::printf("- %p\n", address);
    }
}
}

extern "C" {
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void __real_free(void *);
void *__real__Znwm(size_t);
void __real__ZdlPv(void *);
void __real__ZdlPvm(void *, size_t);

void *__wrap_malloc(size_t bytes)
{
    void *p = __real_malloc(bytes);
    memory_trace::acquire(p, bytes);
    return p;
}

void *__wrap_calloc(size_t count, size_t bytes)
{
    void *p = __real_calloc(count, bytes);
    memory_trace::acquire(p, count * bytes);
    return p;
}

void __wrap_free(void *p)
{
    memory_trace::release(p);
    __real_free(p);
}

// Scalar operator new, operator delete and sized delete (x86-64 Itanium ABI).
void *__wrap__Znwm(size_t bytes)
{
    void *p = __real__Znwm(bytes);
    memory_trace::acquire(p, bytes);
    return p;
}

void __wrap__ZdlPv(void *p)
{
    memory_trace::release(p);
    __real__ZdlPv(p);
}

void __wrap__ZdlPvm(void *p, size_t bytes)
{
    memory_trace::release(p);
    __real__ZdlPvm(p, bytes);
}
}
