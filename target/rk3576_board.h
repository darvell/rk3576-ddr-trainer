#pragma once

#include <stdint.h>

#include "rk3576_ddr/abi.h"
#include "rk3576_ddr/geometry.h"
#include "rk3576_ddr/register_plan.h"

#define RK3576_DDR_BOARD_BUILD_ID "rk3576-ddr-trainer"

typedef struct rk3576_ddr_board_config {
    rk3576_ddr_boot_params boot;
    rk3576_ddr_runtime_geometry geometry;
    rk3576_ddr_addrmap_options addrmap;
    uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS];
    uint32_t mr8_by_channel[RK3576_DDR_MAX_CHANNELS];
    uint32_t sys_sgrf_word0;
    uint32_t debug_clock_hz;
    uintptr_t handoff_entry;
} rk3576_ddr_board_config;

void rk3576_ddr_board_config_init(rk3576_ddr_board_config *config);
