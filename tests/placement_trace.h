#ifndef PLACEMENT_TRACE_H
#define PLACEMENT_TRACE_H

#include <stddef.h>
#include <stdint.h>

// A decision shared by the whole matching group can report its entire word-ID range.
void trace_placements(uint8_t row, uint8_t start, const uint32_t *words, size_t count, bool accepted);
void trace_square_lookup(uint8_t row, uint8_t col);

#endif
