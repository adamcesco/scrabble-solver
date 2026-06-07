#include "board.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint16_t mask;
    const char *row;
} ExpectedConfig;

static uint16_t index_to_config[MAX_NUMBER_OF_START_CONFIGS];
static uint16_t config_to_index[WORD_START_CONFIG_LOOKUP_SIZE];

static const ExpectedConfig expected_configs[] = {
    {UINT16_C(0x0000), "..............."},
    {UINT16_C(0x0001), "S.............."},
    {UINT16_C(0x0009), "S..S..........."},
    {UINT16_C(0x0049), "S..S..S........"},
    {UINT16_C(0x0249), "S..S..S..S....."},
    {UINT16_C(0x1249), "S..S..S..S..S.."},
    {UINT16_C(0x2249), "S..S..S..S...S."},
    {UINT16_C(0x0449), "S..S..S...S...."},
    {UINT16_C(0x2449), "S..S..S...S..S."},
    {UINT16_C(0x0849), "S..S..S....S..."},
    {UINT16_C(0x1049), "S..S..S.....S.."},
    {UINT16_C(0x2049), "S..S..S......S."},
    {UINT16_C(0x0089), "S..S...S......."},
    {UINT16_C(0x0489), "S..S...S..S...."},
    {UINT16_C(0x2489), "S..S...S..S..S."},
    {UINT16_C(0x0889), "S..S...S...S..."},
    {UINT16_C(0x1089), "S..S...S....S.."},
    {UINT16_C(0x2089), "S..S...S.....S."},
    {UINT16_C(0x0109), "S..S....S......"},
    {UINT16_C(0x0909), "S..S....S..S..."},
    {UINT16_C(0x1109), "S..S....S...S.."},
    {UINT16_C(0x2109), "S..S....S....S."},
    {UINT16_C(0x0209), "S..S.....S....."},
    {UINT16_C(0x1209), "S..S.....S..S.."},
    {UINT16_C(0x2209), "S..S.....S...S."},
    {UINT16_C(0x0409), "S..S......S...."},
    {UINT16_C(0x2409), "S..S......S..S."},
    {UINT16_C(0x0809), "S..S.......S..."},
    {UINT16_C(0x1009), "S..S........S.."},
    {UINT16_C(0x2009), "S..S.........S."},
    {UINT16_C(0x0011), "S...S.........."},
    {UINT16_C(0x0091), "S...S..S......."},
    {UINT16_C(0x0491), "S...S..S..S...."},
    {UINT16_C(0x2491), "S...S..S..S..S."},
    {UINT16_C(0x0891), "S...S..S...S..."},
    {UINT16_C(0x1091), "S...S..S....S.."},
    {UINT16_C(0x2091), "S...S..S.....S."},
    {UINT16_C(0x0111), "S...S...S......"},
    {UINT16_C(0x0911), "S...S...S..S..."},
    {UINT16_C(0x1111), "S...S...S...S.."},
    {UINT16_C(0x2111), "S...S...S....S."},
    {UINT16_C(0x0211), "S...S....S....."},
    {UINT16_C(0x1211), "S...S....S..S.."},
    {UINT16_C(0x2211), "S...S....S...S."},
    {UINT16_C(0x0411), "S...S.....S...."},
    {UINT16_C(0x2411), "S...S.....S..S."},
    {UINT16_C(0x0811), "S...S......S..."},
    {UINT16_C(0x1011), "S...S.......S.."},
    {UINT16_C(0x2011), "S...S........S."},
    {UINT16_C(0x0021), "S....S........."},
    {UINT16_C(0x0121), "S....S..S......"},
    {UINT16_C(0x0921), "S....S..S..S..."},
    {UINT16_C(0x1121), "S....S..S...S.."},
    {UINT16_C(0x2121), "S....S..S....S."},
    {UINT16_C(0x0221), "S....S...S....."},
    {UINT16_C(0x1221), "S....S...S..S.."},
    {UINT16_C(0x2221), "S....S...S...S."},
    {UINT16_C(0x0421), "S....S....S...."},
    {UINT16_C(0x2421), "S....S....S..S."},
    {UINT16_C(0x0821), "S....S.....S..."},
    {UINT16_C(0x1021), "S....S......S.."},
    {UINT16_C(0x2021), "S....S.......S."},
    {UINT16_C(0x0041), "S.....S........"},
    {UINT16_C(0x0241), "S.....S..S....."},
    {UINT16_C(0x1241), "S.....S..S..S.."},
    {UINT16_C(0x2241), "S.....S..S...S."},
    {UINT16_C(0x0441), "S.....S...S...."},
    {UINT16_C(0x2441), "S.....S...S..S."},
    {UINT16_C(0x0841), "S.....S....S..."},
    {UINT16_C(0x1041), "S.....S.....S.."},
    {UINT16_C(0x2041), "S.....S......S."},
    {UINT16_C(0x0081), "S......S......."},
    {UINT16_C(0x0481), "S......S..S...."},
    {UINT16_C(0x2481), "S......S..S..S."},
    {UINT16_C(0x0881), "S......S...S..."},
    {UINT16_C(0x1081), "S......S....S.."},
    {UINT16_C(0x2081), "S......S.....S."},
    {UINT16_C(0x0101), "S.......S......"},
    {UINT16_C(0x0901), "S.......S..S..."},
    {UINT16_C(0x1101), "S.......S...S.."},
    {UINT16_C(0x2101), "S.......S....S."},
    {UINT16_C(0x0201), "S........S....."},
    {UINT16_C(0x1201), "S........S..S.."},
    {UINT16_C(0x2201), "S........S...S."},
    {UINT16_C(0x0401), "S.........S...."},
    {UINT16_C(0x2401), "S.........S..S."},
    {UINT16_C(0x0801), "S..........S..."},
    {UINT16_C(0x1001), "S...........S.."},
    {UINT16_C(0x2001), "S............S."},
    {UINT16_C(0x0002), ".S............."},
    {UINT16_C(0x0012), ".S..S.........."},
    {UINT16_C(0x0092), ".S..S..S......."},
    {UINT16_C(0x0492), ".S..S..S..S...."},
    {UINT16_C(0x2492), ".S..S..S..S..S."},
    {UINT16_C(0x0892), ".S..S..S...S..."},
    {UINT16_C(0x1092), ".S..S..S....S.."},
    {UINT16_C(0x2092), ".S..S..S.....S."},
    {UINT16_C(0x0112), ".S..S...S......"},
    {UINT16_C(0x0912), ".S..S...S..S..."},
    {UINT16_C(0x1112), ".S..S...S...S.."},
    {UINT16_C(0x2112), ".S..S...S....S."},
    {UINT16_C(0x0212), ".S..S....S....."},
    {UINT16_C(0x1212), ".S..S....S..S.."},
    {UINT16_C(0x2212), ".S..S....S...S."},
    {UINT16_C(0x0412), ".S..S.....S...."},
    {UINT16_C(0x2412), ".S..S.....S..S."},
    {UINT16_C(0x0812), ".S..S......S..."},
    {UINT16_C(0x1012), ".S..S.......S.."},
    {UINT16_C(0x2012), ".S..S........S."},
    {UINT16_C(0x0022), ".S...S........."},
    {UINT16_C(0x0122), ".S...S..S......"},
    {UINT16_C(0x0922), ".S...S..S..S..."},
    {UINT16_C(0x1122), ".S...S..S...S.."},
    {UINT16_C(0x2122), ".S...S..S....S."},
    {UINT16_C(0x0222), ".S...S...S....."},
    {UINT16_C(0x1222), ".S...S...S..S.."},
    {UINT16_C(0x2222), ".S...S...S...S."},
    {UINT16_C(0x0422), ".S...S....S...."},
    {UINT16_C(0x2422), ".S...S....S..S."},
    {UINT16_C(0x0822), ".S...S.....S..."},
    {UINT16_C(0x1022), ".S...S......S.."},
    {UINT16_C(0x2022), ".S...S.......S."},
    {UINT16_C(0x0042), ".S....S........"},
    {UINT16_C(0x0242), ".S....S..S....."},
    {UINT16_C(0x1242), ".S....S..S..S.."},
    {UINT16_C(0x2242), ".S....S..S...S."},
    {UINT16_C(0x0442), ".S....S...S...."},
    {UINT16_C(0x2442), ".S....S...S..S."},
    {UINT16_C(0x0842), ".S....S....S..."},
    {UINT16_C(0x1042), ".S....S.....S.."},
    {UINT16_C(0x2042), ".S....S......S."},
    {UINT16_C(0x0082), ".S.....S......."},
    {UINT16_C(0x0482), ".S.....S..S...."},
    {UINT16_C(0x2482), ".S.....S..S..S."},
    {UINT16_C(0x0882), ".S.....S...S..."},
    {UINT16_C(0x1082), ".S.....S....S.."},
    {UINT16_C(0x2082), ".S.....S.....S."},
    {UINT16_C(0x0102), ".S......S......"},
    {UINT16_C(0x0902), ".S......S..S..."},
    {UINT16_C(0x1102), ".S......S...S.."},
    {UINT16_C(0x2102), ".S......S....S."},
    {UINT16_C(0x0202), ".S.......S....."},
    {UINT16_C(0x1202), ".S.......S..S.."},
    {UINT16_C(0x2202), ".S.......S...S."},
    {UINT16_C(0x0402), ".S........S...."},
    {UINT16_C(0x2402), ".S........S..S."},
    {UINT16_C(0x0802), ".S.........S..."},
    {UINT16_C(0x1002), ".S..........S.."},
    {UINT16_C(0x2002), ".S...........S."},
    {UINT16_C(0x0004), "..S............"},
    {UINT16_C(0x0024), "..S..S........."},
    {UINT16_C(0x0124), "..S..S..S......"},
    {UINT16_C(0x0924), "..S..S..S..S..."},
    {UINT16_C(0x1124), "..S..S..S...S.."},
    {UINT16_C(0x2124), "..S..S..S....S."},
    {UINT16_C(0x0224), "..S..S...S....."},
    {UINT16_C(0x1224), "..S..S...S..S.."},
    {UINT16_C(0x2224), "..S..S...S...S."},
    {UINT16_C(0x0424), "..S..S....S...."},
    {UINT16_C(0x2424), "..S..S....S..S."},
    {UINT16_C(0x0824), "..S..S.....S..."},
    {UINT16_C(0x1024), "..S..S......S.."},
    {UINT16_C(0x2024), "..S..S.......S."},
    {UINT16_C(0x0044), "..S...S........"},
    {UINT16_C(0x0244), "..S...S..S....."},
    {UINT16_C(0x1244), "..S...S..S..S.."},
    {UINT16_C(0x2244), "..S...S..S...S."},
    {UINT16_C(0x0444), "..S...S...S...."},
    {UINT16_C(0x2444), "..S...S...S..S."},
    {UINT16_C(0x0844), "..S...S....S..."},
    {UINT16_C(0x1044), "..S...S.....S.."},
    {UINT16_C(0x2044), "..S...S......S."},
    {UINT16_C(0x0084), "..S....S......."},
    {UINT16_C(0x0484), "..S....S..S...."},
    {UINT16_C(0x2484), "..S....S..S..S."},
    {UINT16_C(0x0884), "..S....S...S..."},
    {UINT16_C(0x1084), "..S....S....S.."},
    {UINT16_C(0x2084), "..S....S.....S."},
    {UINT16_C(0x0104), "..S.....S......"},
    {UINT16_C(0x0904), "..S.....S..S..."},
    {UINT16_C(0x1104), "..S.....S...S.."},
    {UINT16_C(0x2104), "..S.....S....S."},
    {UINT16_C(0x0204), "..S......S....."},
    {UINT16_C(0x1204), "..S......S..S.."},
    {UINT16_C(0x2204), "..S......S...S."},
    {UINT16_C(0x0404), "..S.......S...."},
    {UINT16_C(0x2404), "..S.......S..S."},
    {UINT16_C(0x0804), "..S........S..."},
    {UINT16_C(0x1004), "..S.........S.."},
    {UINT16_C(0x2004), "..S..........S."},
    {UINT16_C(0x0008), "...S..........."},
    {UINT16_C(0x0048), "...S..S........"},
    {UINT16_C(0x0248), "...S..S..S....."},
    {UINT16_C(0x1248), "...S..S..S..S.."},
    {UINT16_C(0x2248), "...S..S..S...S."},
    {UINT16_C(0x0448), "...S..S...S...."},
    {UINT16_C(0x2448), "...S..S...S..S."},
    {UINT16_C(0x0848), "...S..S....S..."},
    {UINT16_C(0x1048), "...S..S.....S.."},
    {UINT16_C(0x2048), "...S..S......S."},
    {UINT16_C(0x0088), "...S...S......."},
    {UINT16_C(0x0488), "...S...S..S...."},
    {UINT16_C(0x2488), "...S...S..S..S."},
    {UINT16_C(0x0888), "...S...S...S..."},
    {UINT16_C(0x1088), "...S...S....S.."},
    {UINT16_C(0x2088), "...S...S.....S."},
    {UINT16_C(0x0108), "...S....S......"},
    {UINT16_C(0x0908), "...S....S..S..."},
    {UINT16_C(0x1108), "...S....S...S.."},
    {UINT16_C(0x2108), "...S....S....S."},
    {UINT16_C(0x0208), "...S.....S....."},
    {UINT16_C(0x1208), "...S.....S..S.."},
    {UINT16_C(0x2208), "...S.....S...S."},
    {UINT16_C(0x0408), "...S......S...."},
    {UINT16_C(0x2408), "...S......S..S."},
    {UINT16_C(0x0808), "...S.......S..."},
    {UINT16_C(0x1008), "...S........S.."},
    {UINT16_C(0x2008), "...S.........S."},
    {UINT16_C(0x0010), "....S.........."},
    {UINT16_C(0x0090), "....S..S......."},
    {UINT16_C(0x0490), "....S..S..S...."},
    {UINT16_C(0x2490), "....S..S..S..S."},
    {UINT16_C(0x0890), "....S..S...S..."},
    {UINT16_C(0x1090), "....S..S....S.."},
    {UINT16_C(0x2090), "....S..S.....S."},
    {UINT16_C(0x0110), "....S...S......"},
    {UINT16_C(0x0910), "....S...S..S..."},
    {UINT16_C(0x1110), "....S...S...S.."},
    {UINT16_C(0x2110), "....S...S....S."},
    {UINT16_C(0x0210), "....S....S....."},
    {UINT16_C(0x1210), "....S....S..S.."},
    {UINT16_C(0x2210), "....S....S...S."},
    {UINT16_C(0x0410), "....S.....S...."},
    {UINT16_C(0x2410), "....S.....S..S."},
    {UINT16_C(0x0810), "....S......S..."},
    {UINT16_C(0x1010), "....S.......S.."},
    {UINT16_C(0x2010), "....S........S."},
    {UINT16_C(0x0020), ".....S........."},
    {UINT16_C(0x0120), ".....S..S......"},
    {UINT16_C(0x0920), ".....S..S..S..."},
    {UINT16_C(0x1120), ".....S..S...S.."},
    {UINT16_C(0x2120), ".....S..S....S."},
    {UINT16_C(0x0220), ".....S...S....."},
    {UINT16_C(0x1220), ".....S...S..S.."},
    {UINT16_C(0x2220), ".....S...S...S."},
    {UINT16_C(0x0420), ".....S....S...."},
    {UINT16_C(0x2420), ".....S....S..S."},
    {UINT16_C(0x0820), ".....S.....S..."},
    {UINT16_C(0x1020), ".....S......S.."},
    {UINT16_C(0x2020), ".....S.......S."},
    {UINT16_C(0x0040), "......S........"},
    {UINT16_C(0x0240), "......S..S....."},
    {UINT16_C(0x1240), "......S..S..S.."},
    {UINT16_C(0x2240), "......S..S...S."},
    {UINT16_C(0x0440), "......S...S...."},
    {UINT16_C(0x2440), "......S...S..S."},
    {UINT16_C(0x0840), "......S....S..."},
    {UINT16_C(0x1040), "......S.....S.."},
    {UINT16_C(0x2040), "......S......S."},
    {UINT16_C(0x0080), ".......S......."},
    {UINT16_C(0x0480), ".......S..S...."},
    {UINT16_C(0x2480), ".......S..S..S."},
    {UINT16_C(0x0880), ".......S...S..."},
    {UINT16_C(0x1080), ".......S....S.."},
    {UINT16_C(0x2080), ".......S.....S."},
    {UINT16_C(0x0100), "........S......"},
    {UINT16_C(0x0900), "........S..S..."},
    {UINT16_C(0x1100), "........S...S.."},
    {UINT16_C(0x2100), "........S....S."},
    {UINT16_C(0x0200), ".........S....."},
    {UINT16_C(0x1200), ".........S..S.."},
    {UINT16_C(0x2200), ".........S...S."},
    {UINT16_C(0x0400), "..........S...."},
    {UINT16_C(0x2400), "..........S..S."},
    {UINT16_C(0x0800), "...........S..."},
    {UINT16_C(0x1000), "............S.."},
    {UINT16_C(0x2000), ".............S."},
};

static uint16_t row_to_mask(const char row[BOARD_SIZE + 1])
{
    uint16_t mask = 0;

    for (int i = 0; i < BOARD_SIZE; ++i) {
        assert(row[i] == 'S' || row[i] == '.');

        if (row[i] == 'S') {
            mask |= (uint16_t)(1u << i);
        }
    }

    assert(row[BOARD_SIZE] == '\0');

    return mask;
}

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
    init_config_maps(index_to_config, config_to_index);
}

static void hardcoded_index_configs_match_expected_order(void)
{
    size_t expected_count = sizeof(expected_configs) / sizeof(expected_configs[0]);

    assert(expected_count == MAX_NUMBER_OF_START_CONFIGS);

    for (uint16_t index = 0; index < MAX_NUMBER_OF_START_CONFIGS; ++index) {
        const ExpectedConfig expected = expected_configs[index];

        assert(strlen(expected.row) == BOARD_SIZE);
        assert(row_to_mask(expected.row) == expected.mask);
        assert(index_to_config[index] == expected.mask);
    }
}

static void hardcoded_configs_map_to_expected_indexes(void)
{
    for (uint16_t index = 0; index < MAX_NUMBER_OF_START_CONFIGS; ++index) {
        uint16_t mask = expected_configs[index].mask;

        assert(config_to_index[mask] == index);
    }
}

static void lookup_contains_each_valid_config_once(void)
{
    int mapped_count = 0;

    for (uint16_t config = 0; config < WORD_START_CONFIG_LOOKUP_SIZE; ++config) {
        uint16_t index = config_to_index[config];

        if (config_is_valid(config)) {
            assert(index != UINT16_MAX);
            assert(index < MAX_NUMBER_OF_START_CONFIGS);
            assert(index_to_config[index] == config);
            mapped_count++;
        } else {
            assert(index == UINT16_MAX);
        }
    }

    assert(mapped_count == MAX_NUMBER_OF_START_CONFIGS);
}

static void rejects_configs_with_invalid_word_starts(void)
{
    assert(config_to_index[UINT16_C(0x0003)] == UINT16_MAX);
    assert(config_to_index[UINT16_C(0x0005)] == UINT16_MAX);
    assert(config_to_index[UINT16_C(1) << (MAX_START_CONFIG + 1)] == UINT16_MAX);
}

static void run_test(const char *name, void (*test)(void))
{
    setup();
    test();
    printf("PASS %s\n", name);
}

int main(void)
{
    run_test("hardcoded_index_configs_match_expected_order", hardcoded_index_configs_match_expected_order);
    run_test("hardcoded_configs_map_to_expected_indexes", hardcoded_configs_map_to_expected_indexes);
    run_test("lookup_contains_each_valid_config_once", lookup_contains_each_valid_config_once);
    run_test("rejects_configs_with_invalid_word_starts", rejects_configs_with_invalid_word_starts);

    return 0;
}
