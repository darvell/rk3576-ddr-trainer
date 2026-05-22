#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rk3576_ddr/types.h"

typedef enum rk3576_ddr_training_mode {
    RK3576_DDR_TRAINING_TTOT_MR12 = 0x10u,
    RK3576_DDR_TRAINING_DQ_PATTERN = 0x40u,
} rk3576_ddr_training_mode;

typedef enum rk3576_ddr_training_flags {
    RK3576_DDR_TRAINING_FLAG_NONE = 0u,
    RK3576_DDR_TRAINING_FLAG_RUN = 1u,
    RK3576_DDR_TRAINING_FLAG_ENTRY_SETUP = 0x100u,
    RK3576_DDR_TRAINING_FLAG_EXIT_RESTORE = 0x400u,
} rk3576_ddr_training_flags;

typedef struct rk3576_ddr_training_pass {
    rk3576_ddr_channel_id channel;
    uint32_t rank_selector;
    uint32_t rank_slot;
    rk3576_ddr_fsp_index fsp_index;
    rk3576_ddr_training_mode mode;
    uint32_t ttot_enabled;
    uint32_t flags;
} rk3576_ddr_training_pass;

size_t rk3576_ddr_plan_dvfs_training_passes(rk3576_ddr_training_pass *passes, size_t capacity,
                                             const uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS],
                                             const uint32_t rank_count_by_channel[RK3576_DDR_MAX_CHANNELS],
                                             uint32_t rank_slot, rk3576_ddr_fsp_index fsp_index);
