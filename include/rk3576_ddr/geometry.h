#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rk3576_ddr/types.h"

typedef struct rk3576_ddr_addrmap_options {
    uint32_t column_address_bits;
    uint32_t rank_mode_code;
} rk3576_ddr_addrmap_options;

typedef struct rk3576_ddr_geometry_registers {
    uint32_t pmu0_grf_word48;
    uint32_t pmu0_grf_word4c_mr8;
    uint32_t pmu1_grf_addrmap_208;
    uint32_t pmu1_grf_addrmap_20c;
    uint32_t pmu1_grf_addrmap_210;
    uint32_t pmu1_grf_addrmap_214;
    uint64_t cs_size_bytes[RK3576_DDR_MAX_CHANNELS][2];
    uint32_t zq_channel_mask;
    uint32_t final_control_channel_mask;
} rk3576_ddr_geometry_registers;

typedef enum rk3576_ddr_wck_mode_flags {
    RK3576_DDR_WCK_MODE_DDRCTL_ACTIVE_FSP = 1u,
    RK3576_DDR_WCK_MODE_PHY_HIGH_BIT = 2u,
} rk3576_ddr_wck_mode_flags;

typedef struct rk3576_ddr_handoff_gate_step {
    rk3576_ddr_channel_id channel;
    uint32_t ddr_grf_gate_offset;
    uint32_t ddr_grf_gate_value;
    uint32_t pctl_offset;
    uint32_t pctl_clear_mask;
    uint32_t ddr_grf_sideband_offset;
    uint32_t ddr_grf_sideband_value;
} rk3576_ddr_handoff_gate_step;

void rk3576_ddr_build_geometry_registers(rk3576_ddr_geometry_registers *out,
                                         const rk3576_ddr_runtime_geometry *geometry,
                                         const uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS],
                                         const uint32_t mr8_by_channel[RK3576_DDR_MAX_CHANNELS],
                                         const rk3576_ddr_addrmap_options *addrmap_options);

uint32_t rk3576_ddr_capture_lpddr5_wck_mode(uint32_t ddrctl_active_fsp_word, uint32_t ddrphy_cfg_word);

size_t rk3576_ddr_build_handoff_gate_steps(rk3576_ddr_handoff_gate_step *steps, size_t capacity,
                                           const uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS]);
