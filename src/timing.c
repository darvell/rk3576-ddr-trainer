#include "rk3576_ddr/timing.h"

#include "rk3576_ddr/tables.h"

#include <stddef.h>
#include <string.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#define RK3576_DDR_LPDDR4_PHY_FREQ_ROWS_ADDR 0x3ff92e00u
#define RK3576_DDR_LPDDR5_PHY_FREQ_ROWS_ADDR 0x3ff92e68u
#define RK3576_DDR_TIMING_TABLE_SENTINEL_OFFSET 0x10u
#define RK3576_DDR_LPDDR4_TIMING_SPEED_BIN 21u
#define RK3576_DDR_DDR3_TIMING_SPEED_BIN 21u
#define RK3576_DDR_LPDDR5_TIMING_SPEED_BIN 24u
#define RK3576_DDR4_TIMING_SPEED_BIN 24u
#define RK3576_DDR_TIMING_BUS_WIDTH_BASE RK3576_DDR_TIMING_BUS_WIDTH_X8
#define RK3576_DDR_LPDDR5_WCK_MIN_TARGET_MHZ 1600u
#define RK3576_DDR_LPDDR4_BUCKET_COUNT 8u
#define RK3576_DDR_TIMING_TARGET_X2_MULTIPLIER 2u
#define RK3576_DDR_LPDDR5_PROFILE_WCK_RATIO_STRIDE 2u
#define RK3576_DDR_TIMING_NS_PER_US 1000u
#define RK3576_DDR_LPDDR5_CK_SCALE 2000ull

typedef enum rk3576_ddr_lpddr4_timing_bucket {
    RK3576_DDR_LPDDR4_TIMING_BUCKET_0 = 0u,
    RK3576_DDR_LPDDR4_TIMING_BUCKET_1 = 1u,
    RK3576_DDR_LPDDR4_TIMING_BUCKET_2 = 2u,
    RK3576_DDR_LPDDR4_TIMING_BUCKET_3 = 3u,
    RK3576_DDR_LPDDR4_TIMING_BUCKET_4 = 4u,
    RK3576_DDR_LPDDR4_TIMING_BUCKET_5 = 5u,
    RK3576_DDR_LPDDR4_TIMING_BUCKET_6 = 6u,
    RK3576_DDR_LPDDR4_TIMING_BUCKET_7 = 7u,
} rk3576_ddr_lpddr4_timing_bucket;

typedef struct rk3576_ddr_speed_bin_limit {
    uint32_t max_mhz;
    uint32_t speed_bin;
} rk3576_ddr_speed_bin_limit;

static const uint8_t rk3576_ddr_timing_prefix[16] = {
    0x10, 0x00, 0x28, 0xf0, 0xf0, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static uint32_t ceil_div_u32(uint64_t n, uint32_t d)
{
    return (uint32_t)((n + d - 1u) / d);
}

static const uint8_t *timing_prefix_for_seed(const rk3576_ddr_timing_seed *seed)
{
    if (seed->word[RK3576_DDR_TIMING_SEED_TABLE_MARKER] == RK3576_DDR_LPDDR4_PHY_FREQ_ROWS_ADDR - RK3576_DDR_TIMING_TABLE_SENTINEL_OFFSET ||
        seed->word[RK3576_DDR_TIMING_SEED_TABLE_MARKER] == RK3576_DDR_LPDDR5_PHY_FREQ_ROWS_ADDR - RK3576_DDR_TIMING_TABLE_SENTINEL_OFFSET)
        return rk3576_ddr_timing_prefix;
    return 0;
}

static uint32_t speed_bin_for_target(uint32_t target_mhz,
                                     const rk3576_ddr_speed_bin_limit *limits,
                                     size_t limit_count,
                                     uint32_t fallback_speed_bin)
{
    for (size_t i = 0; i < limit_count; ++i) {
        if (target_mhz <= limits[i].max_mhz)
            return limits[i].speed_bin;
    }
    return fallback_speed_bin;
}

static uint32_t lpddr4_speed_bin_for_target(uint32_t target_mhz)
{
    static const rk3576_ddr_speed_bin_limit limits[] = {
        {400u, 1u}, {533u, 4u}, {666u, 8u}, {800u, 12u}, {933u, 16u}, {1066u, 20u},
    };

    return speed_bin_for_target(target_mhz, limits, ARRAY_LEN(limits), RK3576_DDR_LPDDR4_TIMING_SPEED_BIN);
}

static uint32_t lpddr5_speed_bin_for_target(uint32_t target_mhz)
{
    static const rk3576_ddr_speed_bin_limit limits[] = {
        {800u, 2u}, {933u, 5u}, {1066u, 8u}, {1200u, 12u},
        {1333u, 16u}, {1466u, 20u}, {1600u, 23u},
    };

    return speed_bin_for_target(target_mhz, limits, ARRAY_LEN(limits), RK3576_DDR_LPDDR5_TIMING_SPEED_BIN);
}

uint32_t rk3576_ddr_build_timing_descriptor(const rk3576_ddr_timing_seed *seed,
                                            rk3576_ddr_timing_descriptor *descriptor,
                                            uint32_t override_value)
{
    const uint8_t *prefix;
    uint32_t speed_bin;
    rk3576_ddr_dram_type dram_type;
    uint32_t target_mhz;
    uint32_t fill;

    if (!seed || !descriptor)
        return 0;

    prefix = timing_prefix_for_seed(seed);
    speed_bin = seed->word[RK3576_DDR_TIMING_SEED_SPEED_BIN];
    dram_type = seed->word[RK3576_DDR_TIMING_SEED_DRAM_TYPE];
    target_mhz = seed->word[RK3576_DDR_TIMING_SEED_TARGET_MHZ];
    fill = override_value ? override_value : RK3576_DDR_TIMING_DEFAULT_OVERRIDE;

    memset(descriptor, 0, sizeof(*descriptor));

    if (speed_bin == RK3576_DDR_LPDDR4_TIMING_SPEED_BIN && dram_type == RK3576_DDR3)
        speed_bin = lpddr4_speed_bin_for_target(target_mhz);
    else if (speed_bin == RK3576_DDR_LPDDR5_TIMING_SPEED_BIN && dram_type == RK3576_DDR4)
        speed_bin = lpddr5_speed_bin_for_target(target_mhz);

    descriptor->word[RK3576_DDR_TIMING_DESC_SPEED_BIN] = speed_bin;
    descriptor->word[RK3576_DDR_TIMING_DESC_FILL_COUNT] = seed->word[RK3576_DDR_TIMING_SEED_FILL_COUNT];
    if (!override_value && dram_type == RK3576_LPDDR5)
        fill = RK3576_DDR_LPDDR5_TIMING_DEFAULT_OVERRIDE;
    for (uint32_t i = 0; i < descriptor->word[RK3576_DDR_TIMING_DESC_FILL_COUNT] && 2u + i < RK3576_DDR_TIMING_DESCRIPTOR_WORDS; ++i)
        descriptor->word[RK3576_DDR_TIMING_DESC_FILL_BASE + i] = fill;

    descriptor->word[RK3576_DDR_TIMING_DESC_BUS_WIDTH] = RK3576_DDR_TIMING_BUS_WIDTH_BASE << (seed->word[RK3576_DDR_TIMING_SEED_BUS_WIDTH_CODE] >> 4);
    descriptor->word[RK3576_DDR_TIMING_DESC_DRAM_TYPE] = dram_type;
    descriptor->word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] = target_mhz;
    descriptor->word[RK3576_DDR_TIMING_DESC_STATIC_PROFILE] = 1u;

    if (prefix) {
        descriptor->word[RK3576_DDR_TIMING_DESC_DQ_BITS] = prefix[0];
        descriptor->word[RK3576_DDR_TIMING_DESC_PREFIX_WORD14] = prefix[1];
        descriptor->word[RK3576_DDR_TIMING_DESC_PREFIX_WORD20] = prefix[3];
        descriptor->word[RK3576_DDR_TIMING_DESC_PREFIX_WORD19] = prefix[2];
        if ((dram_type != RK3576_DDR4 && target_mhz <= 300u) || (dram_type == RK3576_DDR4 && target_mhz <= 625u)) {
            descriptor->word[RK3576_DDR_TIMING_DESC_LOW_FREQ_FLAG] = 1u;
            descriptor->word[RK3576_DDR_TIMING_DESC_HIGH_FREQ_FLAG] = 0u;
            descriptor->word[RK3576_DDR_TIMING_DESC_PREFIX_WORD20] = 0u;
        } else {
            descriptor->word[RK3576_DDR_TIMING_DESC_LOW_FREQ_FLAG] = 0u;
            descriptor->word[RK3576_DDR_TIMING_DESC_HIGH_FREQ_FLAG] = target_mhz > 533u;
        }
        if (rk3576_ddr_type_uses_static_timing(dram_type)) {
            descriptor->word[RK3576_DDR_TIMING_DESC_ODT_PROFILE] = prefix[7];
            descriptor->word[RK3576_DDR_TIMING_DESC_PREFIX_WORD18] = prefix[8];
            if (dram_type != RK3576_LPDDR5)
                descriptor->word[RK3576_DDR_TIMING_DESC_PREFIX_WORD21] = prefix[4];
        } else if (dram_type == RK3576_DDR4) {
            descriptor->word[RK3576_DDR_TIMING_DESC_ODT_PROFILE] = prefix[7];
        }
    }

    return descriptor->word[RK3576_DDR_TIMING_DESC_PREFIX_WORD19];
}

static unsigned lpddr4_timing_bucket(uint32_t target_mhz, uint32_t vref_x10)
{
    unsigned freq_bucket;
    unsigned vref_bucket;

    if (target_mhz <= 266u)
        freq_bucket = 0u;
    else if (target_mhz <= 533u)
        freq_bucket = 1u;
    else if (target_mhz <= 800u)
        freq_bucket = 2u;
    else if (target_mhz <= 1066u)
        freq_bucket = 3u;
    else if (target_mhz <= 1333u)
        freq_bucket = 4u;
    else if (target_mhz <= 1600u)
        freq_bucket = 5u;
    else if (target_mhz <= 1866u)
        freq_bucket = 6u;
    else
        freq_bucket = 7u;

    if (vref_x10 <= 4u)
        vref_bucket = 0u;
    else if (vref_x10 <= 6u)
        vref_bucket = 1u;
    else if (vref_x10 <= 8u)
        vref_bucket = 2u;
    else if (vref_x10 <= 10u)
        vref_bucket = 3u;
    else if (vref_x10 <= 12u)
        vref_bucket = 4u;
    else if (vref_x10 <= 14u)
        vref_bucket = 5u;
    else if (vref_x10 <= 16u)
        vref_bucket = 6u;
    else
        vref_bucket = 7u;

    return freq_bucket > vref_bucket ? freq_bucket : vref_bucket;
}

static void derive_lpddr4_timing_values(const rk3576_ddr_timing_descriptor *descriptor,
                                        rk3576_ddr_timing_values *values,
                                        uint32_t vref_x10)
{
    static const uint32_t word78[RK3576_DDR_LPDDR4_BUCKET_COUNT] = {0u, 9u, 18u, 27u, 36u, 45u, 54u, 63u};
    static const uint32_t word125[RK3576_DDR_LPDDR4_BUCKET_COUNT] = {4u, 6u, 8u, 10u, 12u, 14u, 16u, 18u};
    static const uint32_t word77_or[RK3576_DDR_LPDDR4_BUCKET_COUNT] = {4u, 20u, 36u, 52u, 68u, 84u, 100u, 116u};
    uint32_t target_mhz = descriptor->word[RK3576_DDR_TIMING_DESC_TARGET_MHZ];
    uint32_t width_is_8 = descriptor->word[RK3576_DDR_TIMING_DESC_BUS_WIDTH] == RK3576_DDR_TIMING_BUS_WIDTH_X8;
    uint32_t has_feature17 = descriptor->word[RK3576_DDR_TIMING_DESC_ODT_PROFILE] != 0u;
    unsigned bucket = lpddr4_timing_bucket(target_mhz, vref_x10);
    uint32_t v;

    memset(values, 0, sizeof(*values));
    values->word[RK3576_DDR_TIMING_VALUE_TARGET_MHZ] = target_mhz;
    values->word[RK3576_DDR_TIMING_VALUE_RESERVED_123] = 0u;
    values->word[RK3576_DDR_TIMING_VALUE_DQ_BITS] = descriptor->word[RK3576_DDR_TIMING_DESC_DQ_BITS];
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR4_WORD14] = bucket < 3u ? 8u : 2u * (bucket + 1u);
    if (bucket > 6u)
        values->word[RK3576_DDR_TIMING_VALUE_LPDDR4_WORD14] = 16u;
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR4_WORD42] = bucket < 3u ? 0u : (bucket < 5u ? 4u : (bucket < 7u ? 6u : 8u));
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR4_WORD43] = bucket < 3u ? 0u : 14u + 2u * bucket;
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR4_WORD78] = word78[bucket];
    values->word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_B] = word125[bucket];
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR4_WORD77] = (descriptor->word[RK3576_DDR_TIMING_DESC_DQ_BITS] == RK3576_DDR_TIMING_BUS_WIDTH_X32) | word77_or[bucket];

    switch (bucket) {
    case RK3576_DDR_LPDDR4_TIMING_BUCKET_0:
        values->word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_A] = 6u;
        values->word[RK3576_DDR_TIMING_VALUE_MODE_REGISTER] = 6u;
        break;
    case RK3576_DDR_LPDDR4_TIMING_BUCKET_1:
        values->word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_A] = has_feature17 ? 12u : 10u;
        values->word[RK3576_DDR_TIMING_VALUE_MODE_REGISTER] = 2u * (width_is_8 + 5u);
        break;
    case RK3576_DDR_LPDDR4_TIMING_BUCKET_2:
        v = has_feature17 ? width_is_8 + 8u : width_is_8 + 7u;
        values->word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_A] = 2u * v;
        values->word[RK3576_DDR_TIMING_VALUE_MODE_REGISTER] = 16u;
        break;
    case RK3576_DDR_LPDDR4_TIMING_BUCKET_3:
        values->word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_A] = has_feature17 ? 2u * (width_is_8 + 11u) : 2u * (width_is_8 + 10u);
        values->word[RK3576_DDR_TIMING_VALUE_MODE_REGISTER] = 2u * (width_is_8 + 10u);
        break;
    case RK3576_DDR_LPDDR4_TIMING_BUCKET_4:
        values->word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_A] = 2u * (has_feature17 ? width_is_8 + 14u : width_is_8 + 12u);
        values->word[RK3576_DDR_TIMING_VALUE_MODE_REGISTER] = 4u * (width_is_8 + 6u);
        break;
    case RK3576_DDR_LPDDR4_TIMING_BUCKET_5:
        values->word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_A] = 4u * (has_feature17 ? width_is_8 + 8u : width_is_8 + 7u);
        values->word[RK3576_DDR_TIMING_VALUE_MODE_REGISTER] = 2u * (width_is_8 + 15u);
        break;
    case RK3576_DDR_LPDDR4_TIMING_BUCKET_6:
        values->word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_A] = has_feature17 ? 4u * (width_is_8 + 9u) : 4u * (width_is_8 + 8u);
        values->word[RK3576_DDR_TIMING_VALUE_MODE_REGISTER] = 4u * width_is_8 + 34u;
        break;
    default:
        values->word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_A] = has_feature17 ? 4u * (width_is_8 + 10u) : 4u * (width_is_8 + 9u);
        values->word[RK3576_DDR_TIMING_VALUE_MODE_REGISTER] = 4u * (width_is_8 + 10u);
        break;
    }
}

static int select_lpddr5_profiles(const rk3576_ddr_timing_descriptor *descriptor,
                                  const rk3576_ddr_lpddr5_mode_profile **mode_profile,
                                  const rk3576_ddr_lpddr5_timing_profile **timing_profile)
{
    uint32_t target_mhz_x2 = RK3576_DDR_TIMING_TARGET_X2_MULTIPLIER * descriptor->word[RK3576_DDR_TIMING_DESC_TARGET_MHZ];

    if (descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_IO_WIDTH] == RK3576_DDR_LPDDR5_LOW_IO_WIDTH) {
        for (size_t i = 0; i < ARRAY_LEN(rk3576_ddr_lpddr5_low_timing_profiles); ++i) {
            if (target_mhz_x2 <= rk3576_ddr_lpddr5_low_timing_profile_thresholds[i]) {
                *mode_profile = &rk3576_ddr_lpddr5_mode_profiles[i];
                *timing_profile = &rk3576_ddr_lpddr5_low_timing_profiles[i];
                return RK3576_DDR_STATUS_OK;
            }
        }
        return RK3576_DDR_STATUS_UNSUPPORTED;
    }

    for (size_t i = 0; i < ARRAY_LEN(rk3576_ddr_lpddr5_high_timing_profiles); ++i) {
        if (target_mhz_x2 <= rk3576_ddr_lpddr5_high_timing_profile_thresholds[i]) {
            *mode_profile = &rk3576_ddr_lpddr5_mode_profiles[i + RK3576_DDR_LPDDR5_LOW_TIMING_PROFILE_COUNT];
            if (descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_DBI] && i >= RK3576_DDR_LPDDR5_LOW_TIMING_PROFILE_COUNT)
                *timing_profile = &rk3576_ddr_lpddr5_high_dbi_timing_profiles[i - RK3576_DDR_LPDDR5_LOW_TIMING_PROFILE_COUNT];
            else
                *timing_profile = &rk3576_ddr_lpddr5_high_timing_profiles[i];
            return RK3576_DDR_STATUS_OK;
        }
    }

    return RK3576_DDR_STATUS_UNSUPPORTED;
}

static uint32_t lpddr5_mode_code(const rk3576_ddr_timing_descriptor *descriptor, uint32_t *profile_column)
{
    if (descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_WCK_ENABLE]) {
        if (descriptor->word[RK3576_DDR_TIMING_DESC_BUS_WIDTH] == RK3576_DDR_TIMING_BUS_WIDTH_X8) {
            *profile_column = 1u;
            return 1u;
        }
        *profile_column = 0u;
        return 0u;
    }
    if (descriptor->word[RK3576_DDR_TIMING_DESC_BUS_WIDTH] == RK3576_DDR_TIMING_BUS_WIDTH_X8 && descriptor->word[RK3576_DDR_TIMING_DESC_ODT_PROFILE]) {
        *profile_column = 1u;
        return 2u;
    }
    *profile_column = descriptor->word[RK3576_DDR_TIMING_DESC_ODT_PROFILE] ? 1u : 0u;
    return descriptor->word[RK3576_DDR_TIMING_DESC_ODT_PROFILE] ? 1u : 0u;
}

int rk3576_ddr_derive_timing_values(const rk3576_ddr_timing_descriptor *descriptor,
                                    rk3576_ddr_timing_values *values,
                                    uint32_t lpddr4_vref_x10)
{
    const rk3576_ddr_lpddr5_mode_profile *mode_profile;
    const rk3576_ddr_lpddr5_timing_profile *timing_profile;
    uint32_t profile_column;
    uint32_t mode;

    if (!descriptor || !values)
        return RK3576_DDR_STATUS_INVALID_ARGUMENT;

    if (descriptor->word[RK3576_DDR_TIMING_DESC_DRAM_TYPE] == RK3576_LPDDR4 || descriptor->word[RK3576_DDR_TIMING_DESC_DRAM_TYPE] == RK3576_LPDDR4X) {
        derive_lpddr4_timing_values(descriptor, values, lpddr4_vref_x10);
        return RK3576_DDR_STATUS_OK;
    }

    if (descriptor->word[RK3576_DDR_TIMING_DESC_DRAM_TYPE] != RK3576_LPDDR5)
        return RK3576_DDR_STATUS_UNSUPPORTED;

    if (descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_WCK_ENABLE] && descriptor->word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] < RK3576_DDR_LPDDR5_WCK_MIN_TARGET_MHZ)
        return RK3576_DDR_STATUS_UNSUPPORTED;
    if (select_lpddr5_profiles(descriptor, &mode_profile, &timing_profile) != RK3576_DDR_STATUS_OK)
        return RK3576_DDR_STATUS_UNSUPPORTED;

    memset(values, 0, sizeof(*values));
    mode = lpddr5_mode_code(descriptor, &profile_column);
    values->word[RK3576_DDR_TIMING_VALUE_TARGET_MHZ] = descriptor->word[RK3576_DDR_TIMING_DESC_TARGET_MHZ];
    values->word[RK3576_DDR_TIMING_VALUE_MEMORY_KIND] = 5u;
    values->word[RK3576_DDR_TIMING_VALUE_CK_PERIOD] = ceil_div_u32(RK3576_DDR_LPDDR5_CK_SCALE * (descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_IO_WIDTH] == RK3576_DDR_LPDDR5_LOW_IO_WIDTH ? descriptor->word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] >> 1 : descriptor->word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] >> 2), RK3576_DDR_TIMING_NS_PER_US);
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR5_WCK_MODE] = (descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_WCK_ENABLE] << 6) | (16u * descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_WCK_RATIO]);
    values->word[RK3576_DDR_TIMING_VALUE_RESERVED_123] = 0u;
    values->word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_A] = timing_profile->word[mode & 3u];
    values->word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_B] = timing_profile->word[RK3576_DDR_LPDDR5_PROFILE_CA_TRAIN_B];
    values->word[RK3576_DDR_TIMING_VALUE_DQ_BITS] = descriptor->word[RK3576_DDR_TIMING_DESC_DQ_BITS];
    values->word[RK3576_DDR_TIMING_VALUE_MODE_REGISTER] = mode_profile->word[RK3576_DDR_LPDDR5_PROFILE_WCK_RATIO_STRIDE * (descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_WCK_RATIO] != 0u) + profile_column];
    values->word[RK3576_DDR_TIMING_VALUE_MODE_FLAGS] = (2u * timing_profile->word[RK3576_DDR_LPDDR5_PROFILE_MODE_FLAG]) | (16u * descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_WRITE_LEVEL]) |
                        (4u * descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_READ_PREAMBLE]) | (mode << 6) | descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_MODE_BITS];
    values->word[RK3576_DDR_TIMING_VALUE_DQ_RATIO0] = descriptor->word[RK3576_DDR_TIMING_DESC_DQ_BITS] >> (descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_IO_WIDTH] == RK3576_DDR_LPDDR5_LOW_IO_WIDTH ? 2u : 3u);
    values->word[RK3576_DDR_TIMING_VALUE_DQ_RATIO1] = values->word[RK3576_DDR_TIMING_VALUE_DQ_RATIO0];
    values->word[RK3576_DDR_TIMING_VALUE_DQ_RATIO2] = values->word[RK3576_DDR_TIMING_VALUE_DQ_RATIO0];
    if (descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_IO_WIDTH] != RK3576_DDR_LPDDR5_LOW_IO_WIDTH && descriptor->word[RK3576_DDR_TIMING_DESC_DQ_BITS] == RK3576_DDR_TIMING_BUS_WIDTH_X16 && descriptor->word[RK3576_DDR_TIMING_DESC_LPDDR5_ALT_LATENCY])
        values->word[RK3576_DDR_TIMING_VALUE_DQ_RATIO1] = 2u;
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR5_WORD134] = timing_profile->word[RK3576_DDR_LPDDR5_PROFILE_WORD7];
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR5_WORD135] = timing_profile->word[RK3576_DDR_LPDDR5_PROFILE_WORD9];
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR5_WORD136] = timing_profile->word[RK3576_DDR_LPDDR5_PROFILE_WORD10];
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR5_WORD137] = timing_profile->word[RK3576_DDR_LPDDR5_PROFILE_WORD11];
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR5_WORD138] = timing_profile->word[RK3576_DDR_LPDDR5_PROFILE_MODE_BASE + (mode & 3u)];
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR5_WORD139] = timing_profile->word[RK3576_DDR_LPDDR5_PROFILE_WORD15];
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR5_WORD140] = timing_profile->word[RK3576_DDR_LPDDR5_PROFILE_WORD16];
    values->word[RK3576_DDR_TIMING_VALUE_LPDDR5_WORD141] = timing_profile->word[RK3576_DDR_LPDDR5_PROFILE_WORD17];
    return RK3576_DDR_STATUS_OK;
}
