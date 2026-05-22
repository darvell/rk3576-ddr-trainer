#include "rk3576_ddr/pctl_phy_plan.h"

#include "rk3576_ddr/tables.h"
#include "rk3576_ddr/types.h"

enum {
    RK3576_DDR_PCTL_LPDDR4_TYPE_BITS = 2u,
    RK3576_DDR_PCTL_LPDDR5_TYPE_BITS = 8u,
    RK3576_DDR_FSP_WINDOW_SHIFT = 20u,
    RK3576_DDR_RUNTIME_TAPS_OFFSET = 59u * sizeof(uint32_t),
    RK3576_DDR_LPDDR5_DELAY_BYTES_BASE = 252u,
    RK3576_DDR_DELAY_BYTES_PER_CHANNEL = 20u,
    RK3576_DDR_PCTL_DELAY_BYTES_OFFSET = 0x0508u,
    RK3576_DDR_PCTL_CORE_OFFSET = 0x10000u,
    RK3576_DDR_PCTL_LPDDR5_CFG_OFFSET = 0x10380u,
    RK3576_DDR_PCTL_LPDDR5_CFG_CLEAR = 0x00001f00u,
    RK3576_DDR_PCTL_LPDDR5_CFG_SET = 0x00003000u,
    RK3576_DDR_PCTL_LPDDR5_CFG_AUX = 0x00000210u,
    RK3576_DDR_PHY_FSP0_A = 0x03c8u,
    RK3576_DDR_PHY_FSP0_B = 0x03d4u,
    RK3576_DDR_PHY_FSP0_C = 0x03d8u,
    RK3576_DDR_PHY_FSP0_D = 0x03dcu,
    RK3576_DDR_PHY_FSPN_MODE_BASE = 0x00d4u,
    RK3576_DDR_PHY_FSPN_AUX0_BASE = 0x0a00u,
    RK3576_DDR_PHY_FSPN_AUX1_BASE = 0x00ccu,
    RK3576_DDR_RATE_PLL_SETTLE_US = 24u,
    RK3576_DDR_RATE_LPDDR5_SETTLE_US = 0x8a2u,
    RK3576_DDR_RATE_EARLY_MR_TIMING_ENABLED = 1u,
};

#define RK3576_DDR_PCTL_INIT_PATCH_TABLE_ADDR 0x3ff9239cu

static int supported_dram_type(rk3576_ddr_dram_type dram_type)
{
    return rk3576_ddr_type_uses_static_timing(dram_type);
}

static uint32_t pctl_dram_type_bits(rk3576_ddr_dram_type dram_type)
{
    if (dram_type == RK3576_LPDDR5)
        return RK3576_DDR_PCTL_LPDDR5_TYPE_BITS;
    if (dram_type == RK3576_LPDDR4 || dram_type == RK3576_LPDDR4X)
        return RK3576_DDR_PCTL_LPDDR4_TYPE_BITS;
    return 0;
}

static void append_pctl_phy_step(rk3576_ddr_pctl_phy_step *steps, size_t capacity, size_t *count,
                                 rk3576_ddr_pctl_phy_phase phase, rk3576_ddr_plan_target target,
                                 rk3576_ddr_channel_id channel, uint32_t offset,
                                 uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
    if (steps && *count < capacity) {
        steps[*count].phase = phase;
        steps[*count].target = target;
        steps[*count].channel = channel;
        steps[*count].offset = offset;
        steps[*count].arg0 = arg0;
        steps[*count].arg1 = arg1;
        steps[*count].arg2 = arg2;
    }
    ++*count;
}

size_t rk3576_ddr_plan_pctl_phy_timing(rk3576_ddr_pctl_phy_step *steps, size_t capacity,
                                        const uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS],
                                        rk3576_ddr_dram_type dram_type, uint32_t apply_init_patch_table,
                                        rk3576_ddr_fsp_index fsp_slot, rk3576_ddr_fsp_index fsp_index,
                                        uint32_t reuse_saved_delay)
{
    size_t count = 0;
    uint32_t fsp_window = fsp_slot << RK3576_DDR_FSP_WINDOW_SHIFT;
    uint32_t patch_count;

    if (!active_channel_map || !supported_dram_type(dram_type))
        return 0;

    patch_count = apply_init_patch_table ? (uint32_t)rk3576_ddr_copy_valid_pctl_init_patches(0, 0) : 0u;

    for (uint32_t slot = 0; slot < RK3576_DDR_MAX_CHANNELS; ++slot) {
        rk3576_ddr_channel_id channel = active_channel_map[slot];

        if (!rk3576_ddr_channel_is_active(channel))
            continue;

        append_pctl_phy_step(steps, capacity, &count, RK3576_DDR_PCTL_PHY_WRITE_PCTL_COMMON,
                             RK3576_DDR_PLAN_TARGET_DDRCTL, channel, fsp_window, dram_type, fsp_slot, fsp_index);
        append_pctl_phy_step(steps, capacity, &count,
                             dram_type == RK3576_LPDDR5 ? RK3576_DDR_PCTL_PHY_WRITE_PCTL_LPDDR5 : RK3576_DDR_PCTL_PHY_WRITE_PCTL_LPDDR4,
                             RK3576_DDR_PLAN_TARGET_DDRCTL, channel, fsp_window + sizeof(uint32_t), dram_type, 0, 0);
        append_pctl_phy_step(steps, capacity, &count, RK3576_DDR_PCTL_PHY_CONFIGURE_CORE,
                             RK3576_DDR_PLAN_TARGET_DDRCTL, channel, RK3576_DDR_PCTL_CORE_OFFSET,
                             pctl_dram_type_bits(dram_type), apply_init_patch_table, 0);

        if (dram_type == RK3576_LPDDR5)
            append_pctl_phy_step(steps, capacity, &count, RK3576_DDR_PCTL_PHY_CONFIGURE_LPDDR5,
                                 RK3576_DDR_PLAN_TARGET_DDRCTL, channel, RK3576_DDR_PCTL_LPDDR5_CFG_OFFSET,
                                 RK3576_DDR_PCTL_LPDDR5_CFG_CLEAR, RK3576_DDR_PCTL_LPDDR5_CFG_SET,
                                 RK3576_DDR_PCTL_LPDDR5_CFG_AUX);

        if (apply_init_patch_table)
            append_pctl_phy_step(steps, capacity, &count, RK3576_DDR_PCTL_PHY_APPLY_INIT_PATCHES,
                                 RK3576_DDR_PLAN_TARGET_PATCH_TABLE, channel, RK3576_DDR_PCTL_INIT_PATCH_TABLE_ADDR,
                                 RK3576_PCTL_INIT_PATCH_ENTRY_COUNT, patch_count,
                                 RK3576_PCTL_INIT_PATCH_VALID_MAX_OFFSET);

        append_pctl_phy_step(steps, capacity, &count, RK3576_DDR_PCTL_PHY_PROGRAM_RUNTIME_TAPS,
                             RK3576_DDR_PLAN_TARGET_RUNTIME, channel, RK3576_DDR_RUNTIME_TAPS_OFFSET,
                             reuse_saved_delay, dram_type, 0);

        if (fsp_index) {
            uint32_t fsp_offset = sizeof(uint32_t) * (fsp_index - 1u);

            append_pctl_phy_step(steps, capacity, &count, RK3576_DDR_PCTL_PHY_PROGRAM_FSPN,
                                 RK3576_DDR_PLAN_TARGET_DDRPHY, channel,
                                 RK3576_DDR_PHY_FSPN_MODE_BASE + fsp_offset, fsp_index,
                                 RK3576_DDR_PHY_FSPN_AUX0_BASE + fsp_offset,
                                 RK3576_DDR_PHY_FSPN_AUX1_BASE + fsp_offset);
        } else {
            append_pctl_phy_step(steps, capacity, &count, RK3576_DDR_PCTL_PHY_PROGRAM_FSP0,
                                 RK3576_DDR_PLAN_TARGET_DDRPHY, channel, RK3576_DDR_PHY_FSP0_A,
                                 RK3576_DDR_PHY_FSP0_B, RK3576_DDR_PHY_FSP0_C,
                                 RK3576_DDR_PHY_FSP0_D);
        }

        append_pctl_phy_step(steps, capacity, &count, RK3576_DDR_PCTL_PHY_PROGRAM_DELAY_BYTES,
                             dram_type == RK3576_LPDDR5 ? RK3576_DDR_PLAN_TARGET_RUNTIME : RK3576_DDR_PLAN_TARGET_DDRCTL,
                             channel,
                             dram_type == RK3576_LPDDR5 ? RK3576_DDR_LPDDR5_DELAY_BYTES_BASE + RK3576_DDR_DELAY_BYTES_PER_CHANNEL * channel : RK3576_DDR_PCTL_DELAY_BYTES_OFFSET,
                             reuse_saved_delay, fsp_slot, 0);
        append_pctl_phy_step(steps, capacity, &count, RK3576_DDR_PCTL_PHY_WAIT_UPDATE,
                             RK3576_DDR_PLAN_TARGET_DDRPHY, channel, RK3576_DDR_PCTL_CORE_OFFSET, 0, 0, 0);
    }

    return count;
}

static void append_rate_step(rk3576_ddr_rate_step *steps, size_t capacity, size_t *count,
                             rk3576_ddr_rate_phase phase, rk3576_ddr_channel_id channel,
                             uint32_t arg0, uint32_t arg1)
{
    if (steps && *count < capacity) {
        steps[*count].phase = phase;
        steps[*count].channel = channel;
        steps[*count].arg0 = arg0;
        steps[*count].arg1 = arg1;
    }
    ++*count;
}

static void append_rate_channel_setup(rk3576_ddr_rate_step *steps, size_t capacity, size_t *count,
                                      rk3576_ddr_channel_id channel, uint32_t apply_common_perf)
{
    append_rate_step(steps, capacity, count, RK3576_DDR_RATE_PROGRAM_PHY_PLL, channel, RK3576_DDR_RATE_PLL_SETTLE_US, 0);
    append_rate_step(steps, capacity, count, RK3576_DDR_RATE_RESET_PHY_AND_PCTL, channel, 0, 0);
    append_rate_step(steps, capacity, count, RK3576_DDR_RATE_APPLY_EARLY_MR_TIMING, channel, RK3576_DDR_RATE_EARLY_MR_TIMING_ENABLED, 0);
    append_rate_step(steps, capacity, count, RK3576_DDR_RATE_CONFIGURE_PCTL, channel, apply_common_perf, 0);
    append_rate_step(steps, capacity, count, RK3576_DDR_RATE_PROGRAM_PHY_TIMING, channel, 0, 0);
    append_rate_step(steps, capacity, count, RK3576_DDR_RATE_APPLY_PCTL_RANK_TABLE, channel, 0, 0);
    append_rate_step(steps, capacity, count, RK3576_DDR_RATE_APPLY_LATE_MR_TIMING, channel, 0, apply_common_perf);
}

size_t rk3576_ddr_plan_rate_reprogram(rk3576_ddr_rate_step *steps, size_t capacity,
                                       const uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS],
                                       rk3576_ddr_dram_type dram_type, uint32_t apply_common_perf)
{
    size_t count = 0;

    if (!active_channel_map || !supported_dram_type(dram_type))
        return 0;

    append_rate_step(steps, capacity, &count, RK3576_DDR_RATE_FORCE_GRF_WINDOW, RK3576_DISABLED_CHANNEL, 1u, 0);

    for (uint32_t slot = 0; slot < RK3576_DDR_MAX_CHANNELS; ++slot) {
        rk3576_ddr_channel_id channel = active_channel_map[slot];

        if (rk3576_ddr_channel_is_active(channel))
            append_rate_channel_setup(steps, capacity, &count, channel, apply_common_perf);
    }

    if (apply_common_perf)
        append_rate_step(steps, capacity, &count, RK3576_DDR_RATE_APPLY_COMMON_PERF, RK3576_DISABLED_CHANNEL, 0, 0);

    for (uint32_t slot = 0; slot < RK3576_DDR_MAX_CHANNELS; ++slot) {
        rk3576_ddr_channel_id channel = active_channel_map[slot];

        if (!rk3576_ddr_channel_is_active(channel))
            continue;
        append_rate_step(steps, capacity, &count, RK3576_DDR_RATE_ENABLE_PHY_UPDATE, channel, 0, 0);
        append_rate_step(steps, capacity, &count, RK3576_DDR_RATE_ENABLE_PCTL_SWCTL, channel, 0, 0);
    }

    if (dram_type == RK3576_LPDDR5)
        append_rate_step(steps, capacity, &count, RK3576_DDR_RATE_WAIT_LPDDR5_SETTLE,
                         RK3576_DISABLED_CHANNEL, RK3576_DDR_RATE_LPDDR5_SETTLE_US, 0);

    for (uint32_t slot = 0; slot < RK3576_DDR_MAX_CHANNELS; ++slot) {
        rk3576_ddr_channel_id channel = active_channel_map[slot];

        if (!rk3576_ddr_channel_is_active(channel))
            continue;
        if (dram_type == RK3576_LPDDR5)
            append_rate_step(steps, capacity, &count, RK3576_DDR_RATE_PROGRAM_LPDDR5_FREQ_BYTES,
                             channel, apply_common_perf, 0);
        else
            append_rate_step(steps, capacity, &count, RK3576_DDR_RATE_PROGRAM_LPDDR4_MR_SHADOW_BYTES,
                             channel, apply_common_perf, 0);
        append_rate_step(steps, capacity, &count, RK3576_DDR_RATE_TOGGLE_PHY_TRAINING_MODE,
                         channel, dram_type, 0);
    }

    append_rate_step(steps, capacity, &count, RK3576_DDR_RATE_FORCE_GRF_WINDOW, RK3576_DISABLED_CHANNEL, 0, 0);

    for (uint32_t slot = 0; slot < RK3576_DDR_MAX_CHANNELS; ++slot) {
        rk3576_ddr_channel_id channel = active_channel_map[slot];

        if (rk3576_ddr_channel_is_active(channel))
            append_rate_step(steps, capacity, &count, RK3576_DDR_RATE_FINAL_FREQ_PARAM, channel, dram_type, 0);
    }

    return count;
}
