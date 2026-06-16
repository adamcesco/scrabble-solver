#include "utils.h"

void print_bits_grouped(FILE *stream, unsigned __int128 value, int bit_count, int group_bits, int low_group_first)
{
    int group_count = bit_count / group_bits;

    for (int group_offset = 0; group_offset < group_count; ++group_offset) {
        int group_index = low_group_first ? group_offset : group_count - 1 - group_offset;
        int group_start_bit = group_index * group_bits;

        for (int bit_offset = group_bits - 1; bit_offset >= 0; --bit_offset) {
            int bit_index = group_start_bit + bit_offset;

            fputc((value & (((unsigned __int128)1) << bit_index)) ? '1' : '0', stream);
        }

        if (group_offset < group_count - 1) {
            fputc(' ', stream);
        }
    }
}

int write_exact(FILE *file, const void *data, size_t size)
{
    return fwrite(data, 1, size, file) == size;
}

int read_exact(FILE *file, void *data, size_t size)
{
    return fread(data, 1, size, file) == size;
}

int write_u32(FILE *file, uint32_t value)
{
    return write_exact(file, &value, sizeof(value));
}

int read_u32(FILE *file, uint32_t *value)
{
    return read_exact(file, value, sizeof(*value));
}

int write_u64(FILE *file, uint64_t value)
{
    return write_exact(file, &value, sizeof(value));
}

int read_u64(FILE *file, uint64_t *value)
{
    return read_exact(file, value, sizeof(*value));
}
