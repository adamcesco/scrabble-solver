#ifndef SRC_SOLVERS_SHARED_PLACEMENT_TRACING_H
#define SRC_SOLVERS_SHARED_PLACEMENT_TRACING_H

#include <stddef.h>
#include <stdint.h>

#ifdef SCRABBLE_TRACE_PLACEMENTS
#include "placement_trace.h"
#else
inline void trace_placements(uint8_t, uint8_t, const uint32_t *, size_t, bool) {}
inline void trace_window(uint8_t, uint8_t, uint8_t, uint8_t, bool) {}
inline void trace_group(size_t, bool) {}
inline void trace_skipped_subset_probe() {}
inline void trace_skipped_merge_comparison() {}
inline void trace_individual_check() {}
inline void trace_gap_check(uint8_t, bool) {}
inline void trace_mask_built(bool, unsigned, bool = false) {}
#endif

#endif
