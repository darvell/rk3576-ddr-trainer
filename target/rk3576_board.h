#pragma once

#include <stdint.h>

#include "rk3576_ddr/abi.h"
#include "rk3576_ddr/geometry.h"
#include "rk3576_ddr/register_plan.h"

#define RK3576_DDR_BOARD_BUILD_ID "rk3576-ddr-trainer"

#define RK3576_DDR_CONFIG_TAGS_ADDR 0x401fe000u
#define RK3576_DDR_DVFS_RECORDS_ADDR 0x40100000u
#define RK3576_DDR_EXPORT_IMAGE_ADDR 0x40104000u
#define RK3576_DDR_BOOT_JUMP_ADDR 0x40060000u
#define RK3576_DDR_BOOT_REQUEST_MAGIC 0x02468aceu
#define RK3576_DDR_BOOT_REQUEST_ACK 0x13579bdfu
#define RK3576_DDR_BOARD_CHANNEL_MASK 0x3u

typedef struct rk3576_ddr_board_config {
    rk3576_ddr_boot_params boot;
    rk3576_ddr_runtime_geometry geometry;
    rk3576_ddr_addrmap_options addrmap;
    uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS];
    uint32_t mr8_by_channel[RK3576_DDR_MAX_CHANNELS];
    uint32_t sys_sgrf_word0;
    uint32_t debug_clock_hz;
    uintptr_t config_tags_addr;
    uintptr_t dvfs_records_addr;
    uintptr_t export_image_addr;
    uintptr_t boot_jump_addr;
} rk3576_ddr_board_config;

void rk3576_ddr_board_config_init(rk3576_ddr_board_config *config);
