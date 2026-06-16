#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static inline unsigned byte_shift_128(unsigned slot, unsigned slot_count)
{
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return 8u * (slot_count - 1u - slot);
#else
    (void)slot_count;
    return 8u * slot;
#endif
}

static inline unsigned byte_shift_64(unsigned slot)
{
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return 8u * (7u - slot);
#else
    return 8u * slot;
#endif
}

static inline uint8_t get_byte_128(unsigned __int128 value, unsigned slot, unsigned slot_count)
{
    return (uint8_t)(value >> byte_shift_128(slot, slot_count));
}

static inline unsigned __int128 put_byte_128(uint8_t value, unsigned slot, unsigned slot_count)
{
    return ((unsigned __int128)value) << byte_shift_128(slot, slot_count);
}

static inline uint8_t get_byte_64(uint64_t value, unsigned slot)
{
    return (uint8_t)(value >> byte_shift_64(slot));
}

static inline uint64_t put_byte_64(uint8_t value, unsigned slot)
{
    return ((uint64_t)value) << byte_shift_64(slot);
}

static inline uint16_t bit_at_u16(uint8_t index)
{
    return (uint16_t)(UINT16_C(1) << index);
}

static inline uint16_t low_bit_mask_u16(uint8_t bit_count)
{
    return (uint16_t)((UINT16_C(1) << bit_count) - 1u);
}

static inline uint16_t bit_span_mask_u16(uint8_t left, uint8_t right)
{
    return (uint16_t)(low_bit_mask_u16((uint8_t)(right - left + 1u)) << left);
}

void print_bits_grouped(FILE *stream, unsigned __int128 value, int bit_count, int group_bits, int low_group_first);

int write_exact(FILE *file, const void *data, size_t size);
int read_exact(FILE *file, void *data, size_t size);
int write_u32(FILE *file, uint32_t value);
int read_u32(FILE *file, uint32_t *value);
int write_u64(FILE *file, uint64_t value);
int read_u64(FILE *file, uint64_t *value);

#endif
