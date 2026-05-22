#pragma once

#include <stdint.h>

#include "rk3576_ddr/types.h"

#define RK3576_DDR_PHY_WORD_COUNT 884u
#define RK3576_DDR_CRU_WORD_COUNT 193u
#define RK3576_DDR_BUILD_CONTEXT_WORD_COUNT 184u
#define RK3576_DDR_CHANNEL_CONTEXT_WORD_COUNT 27u

typedef enum rk3576_ddr_phy_dvfs_mode {
    RK3576_DDR_PHY_DVFS_MODE0 = 0u,
    RK3576_DDR_PHY_DVFS_MODE1 = 1u,
    RK3576_DDR_PHY_DVFS_MODE2 = 2u,
} rk3576_ddr_phy_dvfs_mode;

typedef struct rk3576_ddr_dvfs_reg_value {
    uint32_t off_mode1;
    uint32_t off_mode2;
    uint32_t off_mode0;
    uint32_t value;
} rk3576_ddr_dvfs_reg_value;

typedef struct rk3576_ddr_dvfs_channel_block {
    uint32_t phy_fsp_mode;
    uint32_t phy_training_status[5];
    uint32_t per_rank_mode_pair[2][2];
    uint32_t phy_dfi_select_word;
    uint32_t rank_ca_window_a[2][9];
    uint32_t rank_ca_window_b[2][9];
    uint32_t rank_gate_pair[2][2];
    uint32_t rank_dq_window_a[2][10];
    uint32_t rank_dq_window_b[2][10];
    uint32_t rank_dvfs_delay_pair[2][2];
} rk3576_ddr_dvfs_channel_block;

typedef struct rk3576_ddr_dvfs_channel_tail {
    uint32_t derived_timing_words[11];
    uint32_t reserved_2c;
} rk3576_ddr_dvfs_channel_tail;

typedef struct rk3576_ddr_dvfs_fsp_record {
    uint32_t magic;
    uint32_t actual_mhz;
    rk3576_ddr_dvfs_reg_value phy_reg_entries[RK3576_DVFS_FSP_REG_ENTRY_COUNT];
    uint32_t sentinel_minus1;
    uint32_t reserved_47_61[15];
    uint32_t cru_pll_regs[7];
    uint32_t channel_mode_register_packed[RK3576_DDR_MAX_CHANNELS];
    uint32_t phy_ctrl_bit22_snapshot;
    uint32_t timing_word_147;
    uint32_t reserved_73_77[5];
    uint32_t timing_words_125_128[4];
    uint32_t mr_shadow_words[10];
    rk3576_ddr_dvfs_channel_block channel_blocks[RK3576_DDR_MAX_CHANNELS];
    rk3576_ddr_dvfs_channel_tail channel_tail[RK3576_DDR_MAX_CHANNELS];
} rk3576_ddr_dvfs_fsp_record;

_Static_assert(sizeof(rk3576_ddr_dvfs_reg_value) == 0x10, "DVFS register value row layout");
_Static_assert(sizeof(rk3576_ddr_dvfs_channel_block) == 0x17c, "DVFS channel block layout");
_Static_assert(sizeof(rk3576_ddr_dvfs_channel_tail) == 0x30, "DVFS channel tail layout");
_Static_assert(sizeof(rk3576_ddr_dvfs_fsp_record) == 0x4c8, "DVFS FSP record layout");

void rk3576_ddr_snapshot_dvfs_channel(rk3576_ddr_dvfs_channel_block *block,
                                      const uint32_t phy_words[RK3576_DDR_PHY_WORD_COUNT], uint32_t rank_count);
void rk3576_ddr_copy_dvfs_channel_tail(rk3576_ddr_dvfs_channel_tail *tail,
                                       const uint32_t channel_context_words[RK3576_DDR_CHANNEL_CONTEXT_WORD_COUNT]);
void rk3576_ddr_seed_fsp0_phy_mode(rk3576_ddr_dvfs_fsp_record records[RK3576_DVFS_FSP_RECORD_COUNT],
                                   rk3576_ddr_channel_id channel, uint32_t phy_mode_word);
void rk3576_ddr_build_dvfs_reg_values(rk3576_ddr_dvfs_reg_value entries[RK3576_DVFS_FSP_REG_ENTRY_COUNT],
                                      const uint32_t phy_words[RK3576_DDR_PHY_WORD_COUNT], rk3576_ddr_phy_dvfs_mode phy_dvfs_mode,
                                      uint32_t actual_mhz, uint32_t channel0_rank_count);
void rk3576_ddr_build_dvfs_fsp_record(rk3576_ddr_dvfs_fsp_record *record, uint32_t actual_mhz,
                                      const uint32_t phy_ch0_words[RK3576_DDR_PHY_WORD_COUNT], const uint32_t cru_words[RK3576_DDR_CRU_WORD_COUNT],
                                      const uint32_t build_context_words[RK3576_DDR_BUILD_CONTEXT_WORD_COUNT], rk3576_ddr_phy_dvfs_mode phy_dvfs_mode,
                                      const uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS]);
