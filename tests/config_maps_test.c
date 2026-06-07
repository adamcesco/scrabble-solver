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
    {UINT16_C(0x0000), "..............."}, // 0
    {UINT16_C(0x0001), "S.............."}, // 1
    {UINT16_C(0x0009), "S..S..........."}, // 2
    {UINT16_C(0x0049), "S..S..S........"}, // 3
    {UINT16_C(0x0249), "S..S..S..S....."}, // 4
    {UINT16_C(0x1249), "S..S..S..S..S.."}, // 5
    {UINT16_C(0x2249), "S..S..S..S...S."}, // 6
    {UINT16_C(0x0449), "S..S..S...S...."}, // 7
    {UINT16_C(0x2449), "S..S..S...S..S."}, // 8
    {UINT16_C(0x0849), "S..S..S....S..."}, // 9
    {UINT16_C(0x1049), "S..S..S.....S.."}, // 10
    {UINT16_C(0x2049), "S..S..S......S."}, // 11
    {UINT16_C(0x0089), "S..S...S......."}, // 12
    {UINT16_C(0x0489), "S..S...S..S...."}, // 13
    {UINT16_C(0x2489), "S..S...S..S..S."}, // 14
    {UINT16_C(0x0889), "S..S...S...S..."}, // 15
    {UINT16_C(0x1089), "S..S...S....S.."}, // 16
    {UINT16_C(0x2089), "S..S...S.....S."}, // 17
    {UINT16_C(0x0109), "S..S....S......"}, // 18
    {UINT16_C(0x0909), "S..S....S..S..."}, // 19
    {UINT16_C(0x1109), "S..S....S...S.."}, // 20
    {UINT16_C(0x2109), "S..S....S....S."}, // 21
    {UINT16_C(0x0209), "S..S.....S....."}, // 22
    {UINT16_C(0x1209), "S..S.....S..S.."}, // 23
    {UINT16_C(0x2209), "S..S.....S...S."}, // 24
    {UINT16_C(0x0409), "S..S......S...."}, // 25
    {UINT16_C(0x2409), "S..S......S..S."}, // 26
    {UINT16_C(0x0809), "S..S.......S..."}, // 27
    {UINT16_C(0x1009), "S..S........S.."}, // 28
    {UINT16_C(0x2009), "S..S.........S."}, // 29
    {UINT16_C(0x0011), "S...S.........."}, // 30
    {UINT16_C(0x0091), "S...S..S......."}, // 31
    {UINT16_C(0x0491), "S...S..S..S...."}, // 32
    {UINT16_C(0x2491), "S...S..S..S..S."}, // 33
    {UINT16_C(0x0891), "S...S..S...S..."}, // 34
    {UINT16_C(0x1091), "S...S..S....S.."}, // 35
    {UINT16_C(0x2091), "S...S..S.....S."}, // 36
    {UINT16_C(0x0111), "S...S...S......"}, // 37
    {UINT16_C(0x0911), "S...S...S..S..."}, // 38
    {UINT16_C(0x1111), "S...S...S...S.."}, // 39
    {UINT16_C(0x2111), "S...S...S....S."}, // 40
    {UINT16_C(0x0211), "S...S....S....."}, // 41
    {UINT16_C(0x1211), "S...S....S..S.."}, // 42
    {UINT16_C(0x2211), "S...S....S...S."}, // 43
    {UINT16_C(0x0411), "S...S.....S...."}, // 44
    {UINT16_C(0x2411), "S...S.....S..S."}, // 45
    {UINT16_C(0x0811), "S...S......S..."}, // 46
    {UINT16_C(0x1011), "S...S.......S.."}, // 47
    {UINT16_C(0x2011), "S...S........S."}, // 48
    {UINT16_C(0x0021), "S....S........."}, // 49
    {UINT16_C(0x0121), "S....S..S......"}, // 50
    {UINT16_C(0x0921), "S....S..S..S..."}, // 51
    {UINT16_C(0x1121), "S....S..S...S.."}, // 52
    {UINT16_C(0x2121), "S....S..S....S."}, // 53
    {UINT16_C(0x0221), "S....S...S....."}, // 54
    {UINT16_C(0x1221), "S....S...S..S.."}, // 55
    {UINT16_C(0x2221), "S....S...S...S."}, // 56
    {UINT16_C(0x0421), "S....S....S...."}, // 57
    {UINT16_C(0x2421), "S....S....S..S."}, // 58
    {UINT16_C(0x0821), "S....S.....S..."}, // 59
    {UINT16_C(0x1021), "S....S......S.."}, // 60
    {UINT16_C(0x2021), "S....S.......S."}, // 61
    {UINT16_C(0x0041), "S.....S........"}, // 62
    {UINT16_C(0x0241), "S.....S..S....."}, // 63
    {UINT16_C(0x1241), "S.....S..S..S.."}, // 64
    {UINT16_C(0x2241), "S.....S..S...S."}, // 65
    {UINT16_C(0x0441), "S.....S...S...."}, // 66
    {UINT16_C(0x2441), "S.....S...S..S."}, // 67
    {UINT16_C(0x0841), "S.....S....S..."}, // 68
    {UINT16_C(0x1041), "S.....S.....S.."}, // 69
    {UINT16_C(0x2041), "S.....S......S."}, // 70
    {UINT16_C(0x0081), "S......S......."}, // 71
    {UINT16_C(0x0481), "S......S..S...."}, // 72
    {UINT16_C(0x2481), "S......S..S..S."}, // 73
    {UINT16_C(0x0881), "S......S...S..."}, // 74
    {UINT16_C(0x1081), "S......S....S.."}, // 75
    {UINT16_C(0x2081), "S......S.....S."}, // 76
    {UINT16_C(0x0101), "S.......S......"}, // 77
    {UINT16_C(0x0901), "S.......S..S..."}, // 78
    {UINT16_C(0x1101), "S.......S...S.."}, // 79
    {UINT16_C(0x2101), "S.......S....S."}, // 80
    {UINT16_C(0x0201), "S........S....."}, // 81
    {UINT16_C(0x1201), "S........S..S.."}, // 82
    {UINT16_C(0x2201), "S........S...S."}, // 83
    {UINT16_C(0x0401), "S.........S...."}, // 84
    {UINT16_C(0x2401), "S.........S..S."}, // 85
    {UINT16_C(0x0801), "S..........S..."}, // 86
    {UINT16_C(0x1001), "S...........S.."}, // 87
    {UINT16_C(0x2001), "S............S."}, // 88
    {UINT16_C(0x0002), ".S............."}, // 89
    {UINT16_C(0x0012), ".S..S.........."}, // 90
    {UINT16_C(0x0092), ".S..S..S......."}, // 91
    {UINT16_C(0x0492), ".S..S..S..S...."}, // 92
    {UINT16_C(0x2492), ".S..S..S..S..S."}, // 93
    {UINT16_C(0x0892), ".S..S..S...S..."}, // 94
    {UINT16_C(0x1092), ".S..S..S....S.."}, // 95
    {UINT16_C(0x2092), ".S..S..S.....S."}, // 96
    {UINT16_C(0x0112), ".S..S...S......"}, // 97
    {UINT16_C(0x0912), ".S..S...S..S..."}, // 98
    {UINT16_C(0x1112), ".S..S...S...S.."}, // 99
    {UINT16_C(0x2112), ".S..S...S....S."}, // 100
    {UINT16_C(0x0212), ".S..S....S....."}, // 101
    {UINT16_C(0x1212), ".S..S....S..S.."}, // 102
    {UINT16_C(0x2212), ".S..S....S...S."}, // 103
    {UINT16_C(0x0412), ".S..S.....S...."}, // 104
    {UINT16_C(0x2412), ".S..S.....S..S."}, // 105
    {UINT16_C(0x0812), ".S..S......S..."}, // 106
    {UINT16_C(0x1012), ".S..S.......S.."}, // 107
    {UINT16_C(0x2012), ".S..S........S."}, // 108
    {UINT16_C(0x0022), ".S...S........."}, // 109
    {UINT16_C(0x0122), ".S...S..S......"}, // 110
    {UINT16_C(0x0922), ".S...S..S..S..."}, // 111
    {UINT16_C(0x1122), ".S...S..S...S.."}, // 112
    {UINT16_C(0x2122), ".S...S..S....S."}, // 113
    {UINT16_C(0x0222), ".S...S...S....."}, // 114
    {UINT16_C(0x1222), ".S...S...S..S.."}, // 115
    {UINT16_C(0x2222), ".S...S...S...S."}, // 116
    {UINT16_C(0x0422), ".S...S....S...."}, // 117
    {UINT16_C(0x2422), ".S...S....S..S."}, // 118
    {UINT16_C(0x0822), ".S...S.....S..."}, // 119
    {UINT16_C(0x1022), ".S...S......S.."}, // 120
    {UINT16_C(0x2022), ".S...S.......S."}, // 121
    {UINT16_C(0x0042), ".S....S........"}, // 122
    {UINT16_C(0x0242), ".S....S..S....."}, // 123
    {UINT16_C(0x1242), ".S....S..S..S.."}, // 124
    {UINT16_C(0x2242), ".S....S..S...S."}, // 125
    {UINT16_C(0x0442), ".S....S...S...."}, // 126
    {UINT16_C(0x2442), ".S....S...S..S."}, // 127
    {UINT16_C(0x0842), ".S....S....S..."}, // 128
    {UINT16_C(0x1042), ".S....S.....S.."}, // 129
    {UINT16_C(0x2042), ".S....S......S."}, // 130
    {UINT16_C(0x0082), ".S.....S......."}, // 131
    {UINT16_C(0x0482), ".S.....S..S...."}, // 132
    {UINT16_C(0x2482), ".S.....S..S..S."}, // 133
    {UINT16_C(0x0882), ".S.....S...S..."}, // 134
    {UINT16_C(0x1082), ".S.....S....S.."}, // 135
    {UINT16_C(0x2082), ".S.....S.....S."}, // 136
    {UINT16_C(0x0102), ".S......S......"}, // 137
    {UINT16_C(0x0902), ".S......S..S..."}, // 138
    {UINT16_C(0x1102), ".S......S...S.."}, // 139
    {UINT16_C(0x2102), ".S......S....S."}, // 140
    {UINT16_C(0x0202), ".S.......S....."}, // 141
    {UINT16_C(0x1202), ".S.......S..S.."}, // 142
    {UINT16_C(0x2202), ".S.......S...S."}, // 143
    {UINT16_C(0x0402), ".S........S...."}, // 144
    {UINT16_C(0x2402), ".S........S..S."}, // 145
    {UINT16_C(0x0802), ".S.........S..."}, // 146
    {UINT16_C(0x1002), ".S..........S.."}, // 147
    {UINT16_C(0x2002), ".S...........S."}, // 148
    {UINT16_C(0x0004), "..S............"}, // 149
    {UINT16_C(0x0024), "..S..S........."}, // 150
    {UINT16_C(0x0124), "..S..S..S......"}, // 151
    {UINT16_C(0x0924), "..S..S..S..S..."}, // 152
    {UINT16_C(0x1124), "..S..S..S...S.."}, // 153
    {UINT16_C(0x2124), "..S..S..S....S."}, // 154
    {UINT16_C(0x0224), "..S..S...S....."}, // 155
    {UINT16_C(0x1224), "..S..S...S..S.."}, // 156
    {UINT16_C(0x2224), "..S..S...S...S."}, // 157
    {UINT16_C(0x0424), "..S..S....S...."}, // 158
    {UINT16_C(0x2424), "..S..S....S..S."}, // 159
    {UINT16_C(0x0824), "..S..S.....S..."}, // 160
    {UINT16_C(0x1024), "..S..S......S.."}, // 161
    {UINT16_C(0x2024), "..S..S.......S."}, // 162
    {UINT16_C(0x0044), "..S...S........"}, // 163
    {UINT16_C(0x0244), "..S...S..S....."}, // 164
    {UINT16_C(0x1244), "..S...S..S..S.."}, // 165
    {UINT16_C(0x2244), "..S...S..S...S."}, // 166
    {UINT16_C(0x0444), "..S...S...S...."}, // 167
    {UINT16_C(0x2444), "..S...S...S..S."}, // 168
    {UINT16_C(0x0844), "..S...S....S..."}, // 169
    {UINT16_C(0x1044), "..S...S.....S.."}, // 170
    {UINT16_C(0x2044), "..S...S......S."}, // 171
    {UINT16_C(0x0084), "..S....S......."}, // 172
    {UINT16_C(0x0484), "..S....S..S...."}, // 173
    {UINT16_C(0x2484), "..S....S..S..S."}, // 174
    {UINT16_C(0x0884), "..S....S...S..."}, // 175
    {UINT16_C(0x1084), "..S....S....S.."}, // 176
    {UINT16_C(0x2084), "..S....S.....S."}, // 177
    {UINT16_C(0x0104), "..S.....S......"}, // 178
    {UINT16_C(0x0904), "..S.....S..S..."}, // 179
    {UINT16_C(0x1104), "..S.....S...S.."}, // 180
    {UINT16_C(0x2104), "..S.....S....S."}, // 181
    {UINT16_C(0x0204), "..S......S....."}, // 182
    {UINT16_C(0x1204), "..S......S..S.."}, // 183
    {UINT16_C(0x2204), "..S......S...S."}, // 184
    {UINT16_C(0x0404), "..S.......S...."}, // 185
    {UINT16_C(0x2404), "..S.......S..S."}, // 186
    {UINT16_C(0x0804), "..S........S..."}, // 187
    {UINT16_C(0x1004), "..S.........S.."}, // 188
    {UINT16_C(0x2004), "..S..........S."}, // 189
    {UINT16_C(0x0008), "...S..........."}, // 190
    {UINT16_C(0x0048), "...S..S........"}, // 191
    {UINT16_C(0x0248), "...S..S..S....."}, // 192
    {UINT16_C(0x1248), "...S..S..S..S.."}, // 193
    {UINT16_C(0x2248), "...S..S..S...S."}, // 194
    {UINT16_C(0x0448), "...S..S...S...."}, // 195
    {UINT16_C(0x2448), "...S..S...S..S."}, // 196
    {UINT16_C(0x0848), "...S..S....S..."}, // 197
    {UINT16_C(0x1048), "...S..S.....S.."}, // 198
    {UINT16_C(0x2048), "...S..S......S."}, // 199
    {UINT16_C(0x0088), "...S...S......."}, // 200
    {UINT16_C(0x0488), "...S...S..S...."}, // 201
    {UINT16_C(0x2488), "...S...S..S..S."}, // 202
    {UINT16_C(0x0888), "...S...S...S..."}, // 203
    {UINT16_C(0x1088), "...S...S....S.."}, // 204
    {UINT16_C(0x2088), "...S...S.....S."}, // 205
    {UINT16_C(0x0108), "...S....S......"}, // 206
    {UINT16_C(0x0908), "...S....S..S..."}, // 207
    {UINT16_C(0x1108), "...S....S...S.."}, // 208
    {UINT16_C(0x2108), "...S....S....S."}, // 209
    {UINT16_C(0x0208), "...S.....S....."}, // 210
    {UINT16_C(0x1208), "...S.....S..S.."}, // 211
    {UINT16_C(0x2208), "...S.....S...S."}, // 212
    {UINT16_C(0x0408), "...S......S...."}, // 213
    {UINT16_C(0x2408), "...S......S..S."}, // 214
    {UINT16_C(0x0808), "...S.......S..."}, // 215
    {UINT16_C(0x1008), "...S........S.."}, // 216
    {UINT16_C(0x2008), "...S.........S."}, // 217
    {UINT16_C(0x0010), "....S.........."}, // 218
    {UINT16_C(0x0090), "....S..S......."}, // 219
    {UINT16_C(0x0490), "....S..S..S...."}, // 220
    {UINT16_C(0x2490), "....S..S..S..S."}, // 221
    {UINT16_C(0x0890), "....S..S...S..."}, // 222
    {UINT16_C(0x1090), "....S..S....S.."}, // 223
    {UINT16_C(0x2090), "....S..S.....S."}, // 224
    {UINT16_C(0x0110), "....S...S......"}, // 225
    {UINT16_C(0x0910), "....S...S..S..."}, // 226
    {UINT16_C(0x1110), "....S...S...S.."}, // 227
    {UINT16_C(0x2110), "....S...S....S."}, // 228
    {UINT16_C(0x0210), "....S....S....."}, // 229
    {UINT16_C(0x1210), "....S....S..S.."}, // 230
    {UINT16_C(0x2210), "....S....S...S."}, // 231
    {UINT16_C(0x0410), "....S.....S...."}, // 232
    {UINT16_C(0x2410), "....S.....S..S."}, // 233
    {UINT16_C(0x0810), "....S......S..."}, // 234
    {UINT16_C(0x1010), "....S.......S.."}, // 235
    {UINT16_C(0x2010), "....S........S."}, // 236
    {UINT16_C(0x0020), ".....S........."}, // 237
    {UINT16_C(0x0120), ".....S..S......"}, // 238
    {UINT16_C(0x0920), ".....S..S..S..."}, // 239
    {UINT16_C(0x1120), ".....S..S...S.."}, // 240
    {UINT16_C(0x2120), ".....S..S....S."}, // 241
    {UINT16_C(0x0220), ".....S...S....."}, // 242
    {UINT16_C(0x1220), ".....S...S..S.."}, // 243
    {UINT16_C(0x2220), ".....S...S...S."}, // 244
    {UINT16_C(0x0420), ".....S....S...."}, // 245
    {UINT16_C(0x2420), ".....S....S..S."}, // 246
    {UINT16_C(0x0820), ".....S.....S..."}, // 247
    {UINT16_C(0x1020), ".....S......S.."}, // 248
    {UINT16_C(0x2020), ".....S.......S."}, // 249
    {UINT16_C(0x0040), "......S........"}, // 250
    {UINT16_C(0x0240), "......S..S....."}, // 251
    {UINT16_C(0x1240), "......S..S..S.."}, // 252
    {UINT16_C(0x2240), "......S..S...S."}, // 253
    {UINT16_C(0x0440), "......S...S...."}, // 254
    {UINT16_C(0x2440), "......S...S..S."}, // 255
    {UINT16_C(0x0840), "......S....S..."}, // 256
    {UINT16_C(0x1040), "......S.....S.."}, // 257
    {UINT16_C(0x2040), "......S......S."}, // 258
    {UINT16_C(0x0080), ".......S......."}, // 259
    {UINT16_C(0x0480), ".......S..S...."}, // 260
    {UINT16_C(0x2480), ".......S..S..S."}, // 261
    {UINT16_C(0x0880), ".......S...S..."}, // 262
    {UINT16_C(0x1080), ".......S....S.."}, // 263
    {UINT16_C(0x2080), ".......S.....S."}, // 264
    {UINT16_C(0x0100), "........S......"}, // 265
    {UINT16_C(0x0900), "........S..S..."}, // 266
    {UINT16_C(0x1100), "........S...S.."}, // 267
    {UINT16_C(0x2100), "........S....S."}, // 268
    {UINT16_C(0x0200), ".........S....."}, // 269
    {UINT16_C(0x1200), ".........S..S.."}, // 270
    {UINT16_C(0x2200), ".........S...S."}, // 271
    {UINT16_C(0x0400), "..........S...."}, // 272
    {UINT16_C(0x2400), "..........S..S."}, // 273
    {UINT16_C(0x0800), "...........S..."}, // 274
    {UINT16_C(0x1000), "............S.."}, // 275
    {UINT16_C(0x2000), ".............S."}, // 276
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
