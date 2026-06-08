#include "board.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint16_t config_to_start_positions[WORD_START_CONFIG_LOOKUP_SIZE][MAX_NUMBER_OF_WORDS_PER_ROW + 1];

static int config_is_valid(uint16_t config)
{
    int previous_start = -NEXT_START_CONFIG_OFFSET;

    for (int start = 0; start < BOARD_SIZE; ++start) {
        if ((config & (uint16_t)(1u << start)) == 0) {
            continue;
        }

        if (start > MAX_START_CONFIG) {
            return 0;
        }

        if (start - previous_start < NEXT_START_CONFIG_OFFSET) {
            return 0;
        }

        previous_start = start;
    }

    return 1;
}

static void setup(void)
{
    init_config_map(config_to_start_positions);
}

static void maps_configs_to_start_positions(void)
{
    uint16_t empty_config = UINT16_C(0x0000);
    uint16_t one_start_config = UINT16_C(1) << 7;
    uint16_t multi_start_config = (uint16_t)((1u << 2) | (1u << 6) | (1u << 11));

    assert(config_to_start_positions[empty_config][0] == 0);
    for (size_t i = 1; i <= MAX_NUMBER_OF_WORDS_PER_ROW; ++i) {
        assert(config_to_start_positions[empty_config][i] == WORD_START_POSITION_UNUSED);
    }

    assert(config_to_start_positions[one_start_config][0] == 1);
    assert(config_to_start_positions[one_start_config][1] == 7);
    for (size_t i = 2; i <= MAX_NUMBER_OF_WORDS_PER_ROW; ++i) {
        assert(config_to_start_positions[one_start_config][i] == WORD_START_POSITION_UNUSED);
    }

    assert(config_to_start_positions[multi_start_config][0] == 3);
    assert(config_to_start_positions[multi_start_config][1] == 2);
    assert(config_to_start_positions[multi_start_config][2] == 6);
    assert(config_to_start_positions[multi_start_config][3] == 11);
    for (size_t i = 4; i <= MAX_NUMBER_OF_WORDS_PER_ROW; ++i) {
        assert(config_to_start_positions[multi_start_config][i] == WORD_START_POSITION_UNUSED);
    }
}

static void maps_each_valid_config_once(void)
{
    int mapped_count = 0;

    for (uint16_t config = 0; config < WORD_START_CONFIG_LOOKUP_SIZE; ++config) {
        if (!config_is_valid(config)) {
            continue;
        }

        uint16_t expected_count = 0;

        for (uint16_t start = 0; start < BOARD_SIZE; ++start) {
            if ((config & (uint16_t)(1u << start)) == 0) {
                continue;
            }

            expected_count++;
            assert(config_to_start_positions[config][expected_count] == start);
        }

        assert(config_to_start_positions[config][0] == expected_count);
        for (size_t i = expected_count + 1; i <= MAX_NUMBER_OF_WORDS_PER_ROW; ++i) {
            assert(config_to_start_positions[config][i] == WORD_START_POSITION_UNUSED);
        }

        mapped_count++;
    }

    assert(mapped_count == MAX_NUMBER_OF_START_CONFIGS);
}

static void leaves_invalid_config_start_positions_empty(void)
{
    for (uint16_t config = 0; config < WORD_START_CONFIG_LOOKUP_SIZE; ++config) {
        if (config_is_valid(config)) {
            continue;
        }

        assert(config_to_start_positions[config][0] == 0);
        for (size_t i = 1; i <= MAX_NUMBER_OF_WORDS_PER_ROW; ++i) {
            assert(config_to_start_positions[config][i] == WORD_START_POSITION_UNUSED);
        }
    }
}

static void run_test(const char *name, void (*test)(void))
{
    setup();
    test();
    printf("PASS %s\n", name);
}

int main(void)
{
    run_test("maps_configs_to_start_positions", maps_configs_to_start_positions);
    run_test("maps_each_valid_config_once", maps_each_valid_config_once);
    run_test("leaves_invalid_config_start_positions_empty", leaves_invalid_config_start_positions_empty);

    return 0;
}
