#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rk3576_ddr/types.h"

typedef enum rk3576_ddr_plan_target {
    RK3576_DDR_PLAN_TARGET_DDRCTL = 1u,
    RK3576_DDR_PLAN_TARGET_DDRPHY = 2u,
    RK3576_DDR_PLAN_TARGET_RUNTIME = 3u,
    RK3576_DDR_PLAN_TARGET_PATCH_TABLE = 4u,
} rk3576_ddr_plan_target;

typedef enum rk3576_ddr_pctl_phy_phase {
    RK3576_DDR_PCTL_PHY_WRITE_PCTL_COMMON = 1u,
    RK3576_DDR_PCTL_PHY_WRITE_PCTL_LPDDR4,
    RK3576_DDR_PCTL_PHY_WRITE_PCTL_LPDDR5,
    RK3576_DDR_PCTL_PHY_CONFIGURE_CORE,
    RK3576_DDR_PCTL_PHY_CONFIGURE_LPDDR5,
    RK3576_DDR_PCTL_PHY_APPLY_INIT_PATCHES,
    RK3576_DDR_PCTL_PHY_PROGRAM_RUNTIME_TAPS,
    RK3576_DDR_PCTL_PHY_PROGRAM_FSP0,
    RK3576_DDR_PCTL_PHY_PROGRAM_FSPN,
    RK3576_DDR_PCTL_PHY_PROGRAM_DELAY_BYTES,
    RK3576_DDR_PCTL_PHY_WAIT_UPDATE,
} rk3576_ddr_pctl_phy_phase;

typedef struct rk3576_ddr_pctl_phy_step {
    rk3576_ddr_pctl_phy_phase phase;
    rk3576_ddr_plan_target target;
    rk3576_ddr_channel_id channel;
    uint32_t offset;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t arg2;
} rk3576_ddr_pctl_phy_step;

typedef enum rk3576_ddr_rate_phase {
    RK3576_DDR_RATE_FORCE_GRF_WINDOW = 1u,
    RK3576_DDR_RATE_PROGRAM_PHY_PLL,
    RK3576_DDR_RATE_RESET_PHY_AND_PCTL,
    RK3576_DDR_RATE_APPLY_EARLY_MR_TIMING,
    RK3576_DDR_RATE_CONFIGURE_PCTL,
    RK3576_DDR_RATE_PROGRAM_PHY_TIMING,
    RK3576_DDR_RATE_APPLY_PCTL_RANK_TABLE,
    RK3576_DDR_RATE_APPLY_LATE_MR_TIMING,
    RK3576_DDR_RATE_APPLY_COMMON_PERF,
    RK3576_DDR_RATE_ENABLE_PHY_UPDATE,
    RK3576_DDR_RATE_ENABLE_PCTL_SWCTL,
    RK3576_DDR_RATE_WAIT_LPDDR5_SETTLE,
    RK3576_DDR_RATE_PROGRAM_LPDDR5_FREQ_BYTES,
    RK3576_DDR_RATE_PROGRAM_LPDDR4_MR_SHADOW_BYTES,
    RK3576_DDR_RATE_TOGGLE_PHY_TRAINING_MODE,
    RK3576_DDR_RATE_FINAL_FREQ_PARAM,
} rk3576_ddr_rate_phase;

typedef struct rk3576_ddr_rate_step {
    rk3576_ddr_rate_phase phase;
    rk3576_ddr_channel_id channel;
    uint32_t arg0;
    uint32_t arg1;
} rk3576_ddr_rate_step;

size_t rk3576_ddr_plan_pctl_phy_timing(rk3576_ddr_pctl_phy_step *steps, size_t capacity,
                                        const uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS],
                                        rk3576_ddr_dram_type dram_type, uint32_t apply_init_patch_table,
                                        rk3576_ddr_fsp_index fsp_slot, rk3576_ddr_fsp_index fsp_index,
                                        uint32_t reuse_saved_delay);

size_t rk3576_ddr_plan_rate_reprogram(rk3576_ddr_rate_step *steps, size_t capacity,
                                       const uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS],
                                       rk3576_ddr_dram_type dram_type, uint32_t apply_common_perf);
