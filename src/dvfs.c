#include "rk3576_ddr/dvfs.h"

#include "rk3576_ddr/tables.h"

#include <string.h>

enum {
    RK3576_DDR_MAX_CAPTURED_RANKS = 2u,
    RK3576_DDR_TRAIN_STATUS_BASE_WORD = 748u,
    RK3576_DDR_TRAIN_STATUS_WORDS = 5u,
    RK3576_DDR_PHY_DFI_SELECT_WORD = 27u,
    RK3576_DDR_DVFS_TAIL_SOURCE_OFFSET = 15u,
    RK3576_DDR_DVFS_TAIL_WORDS = 11u,
    RK3576_DDR_PHY_FSP_MODE_SHIFT = 21u,
    RK3576_DDR_DVFS_INVALID_OFFSET = 0xffffffffu,
    RK3576_DDR_DVFS_HIGH_FREQ_THRESHOLD_MHZ = 1065u,
    RK3576_DDR_DVFS_SINGLE_RANK_OR_MASK = 0x400u,
    RK3576_DDR_DVFS_DUAL_RANK_OR_MASK = 0xc00u,
    RK3576_DDR_DVFS_HIGH_FREQ_MASKED_OFFSET = 0x0a48u,
    RK3576_DDR_DVFS_HIGH_FREQ_OFFSET_MASK = 0xffffffefu,
    RK3576_DDR_CRU_PLL_ALT_SELECT_WORD = 192u,
    RK3576_DDR_CRU_ALT_BASE_WORD = 8u,
    RK3576_DDR_CRU_PLL_WORDS = 7u,
    RK3576_DDR_CONTEXT_CHANNEL0_RANK_COUNT = 66u,
    RK3576_DDR_CONTEXT_MR_LOW_BASE = 162u,
    RK3576_DDR_CONTEXT_MR_HIGH_BASE = 164u,
    RK3576_DDR_CONTEXT_TIMING_WORD_147 = 147u,
    RK3576_DDR_CONTEXT_TIMING_125_BASE = 125u,
    RK3576_DDR_CONTEXT_TIMING_125_COUNT = 4u,
    RK3576_DDR_CONTEXT_MR_SHADOW_BASE = 129u,
    RK3576_DDR_CONTEXT_MR_SHADOW_COUNT = 10u,
    RK3576_DDR_PHY_CTRL_BIT22 = 0x00400000u,
};

static void copy_indexed_words(uint32_t *dst, const uint32_t *src, const uint16_t *indices, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        dst[i] = src[indices[i]];
}

void rk3576_ddr_snapshot_dvfs_channel(rk3576_ddr_dvfs_channel_block *block,
                                      const uint32_t phy_words[RK3576_DDR_PHY_WORD_COUNT], uint32_t rank_count)
{
    static const uint16_t mode_pair[RK3576_DDR_MAX_CAPTURED_RANKS][2] = {{60, 63}, {377, 380}};
    static const uint16_t ca_a[RK3576_DDR_MAX_CAPTURED_RANKS][9] = {
        {99, 100, 103, 106, 109, 112, 115, 118, 121},
        {475, 476, 477, 478, 479, 480, 481, 482, 483},
    };
    static const uint16_t ca_b[RK3576_DDR_MAX_CAPTURED_RANKS][9] = {
        {388, 389, 392, 395, 398, 401, 404, 407, 410},
        {484, 485, 486, 487, 488, 489, 490, 491, 492},
    };
    static const uint16_t gate_pair[RK3576_DDR_MAX_CAPTURED_RANKS][2] = {{349, 352}, {473, 474}};
    static const uint16_t dq_a[RK3576_DDR_MAX_CAPTURED_RANKS][10] = {
        {124, 127, 130, 133, 136, 139, 142, 145, 148, 149},
        {260, 263, 266, 269, 272, 275, 278, 281, 284, 285},
    };
    static const uint16_t dq_b[RK3576_DDR_MAX_CAPTURED_RANKS][10] = {
        {292, 295, 298, 301, 304, 307, 310, 313, 316, 317},
        {320, 323, 326, 329, 332, 335, 338, 341, 344, 345},
    };
    static const uint16_t delay_pair[RK3576_DDR_MAX_CAPTURED_RANKS][2] = {{880, 881}, {882, 883}};
    uint32_t fsp_mode = block->phy_fsp_mode;

    if (rank_count > RK3576_DDR_MAX_CAPTURED_RANKS)
        rank_count = RK3576_DDR_MAX_CAPTURED_RANKS;

    memset(block, 0, sizeof(*block));
    block->phy_fsp_mode = fsp_mode;

    for (uint32_t i = 0; i < RK3576_DDR_TRAIN_STATUS_WORDS; ++i)
        block->phy_training_status[i] = phy_words[RK3576_DDR_TRAIN_STATUS_BASE_WORD + i];
    block->phy_dfi_select_word = phy_words[RK3576_DDR_PHY_DFI_SELECT_WORD];

    for (uint32_t rank = 0; rank < rank_count; ++rank) {
        copy_indexed_words(block->per_rank_mode_pair[rank], phy_words, mode_pair[rank], 2);
        copy_indexed_words(block->rank_ca_window_a[rank], phy_words, ca_a[rank], 9);
        copy_indexed_words(block->rank_ca_window_b[rank], phy_words, ca_b[rank], 9);
        copy_indexed_words(block->rank_gate_pair[rank], phy_words, gate_pair[rank], 2);
        copy_indexed_words(block->rank_dq_window_a[rank], phy_words, dq_a[rank], 10);
        copy_indexed_words(block->rank_dq_window_b[rank], phy_words, dq_b[rank], 10);
        copy_indexed_words(block->rank_dvfs_delay_pair[rank], phy_words, delay_pair[rank], 2);
    }
}

void rk3576_ddr_copy_dvfs_channel_tail(rk3576_ddr_dvfs_channel_tail *tail,
                                       const uint32_t channel_context_words[27])
{
    memset(tail, 0, sizeof(*tail));
    for (uint32_t i = 0; i < RK3576_DDR_DVFS_TAIL_WORDS; ++i)
        tail->derived_timing_words[i] = channel_context_words[RK3576_DDR_DVFS_TAIL_SOURCE_OFFSET + i];
}

void rk3576_ddr_seed_fsp0_phy_mode(rk3576_ddr_dvfs_fsp_record records[RK3576_DVFS_FSP_RECORD_COUNT],
                                   rk3576_ddr_channel_id channel, uint32_t phy_mode_word)
{
    if (!rk3576_ddr_channel_is_active(channel))
        return;

    for (uint32_t i = 0; i < RK3576_DVFS_FSP_RECORD_COUNT; ++i)
        records[i].channel_blocks[channel].phy_fsp_mode = phy_mode_word >> RK3576_DDR_PHY_FSP_MODE_SHIFT;
}

void rk3576_ddr_build_dvfs_reg_values(rk3576_ddr_dvfs_reg_value entries[RK3576_DVFS_FSP_REG_ENTRY_COUNT],
                                      const uint32_t phy_words[RK3576_DDR_PHY_WORD_COUNT], rk3576_ddr_phy_dvfs_mode phy_dvfs_mode,
                                      uint32_t actual_mhz, uint32_t channel0_rank_count)
{
    uint32_t high_freq_rank_mask = channel0_rank_count == 1u ? RK3576_DDR_DVFS_SINGLE_RANK_OR_MASK :
                                                              RK3576_DDR_DVFS_DUAL_RANK_OR_MASK;

    for (uint32_t i = 0; i < RK3576_DVFS_FSP_REG_ENTRY_COUNT; ++i) {
        uint32_t selected_offset;

        entries[i].off_mode1 = rk3576_ddr_dvfs_fsp_reg_offset_table[i].off_mode1;
        entries[i].off_mode2 = rk3576_ddr_dvfs_fsp_reg_offset_table[i].off_mode2;
        entries[i].off_mode0 = rk3576_ddr_dvfs_fsp_reg_offset_table[i].off_mode0;

        switch (phy_dvfs_mode) {
        case RK3576_DDR_PHY_DVFS_MODE1:
            selected_offset = entries[i].off_mode1;
            break;
        case RK3576_DDR_PHY_DVFS_MODE2:
            selected_offset = entries[i].off_mode2;
            break;
        case RK3576_DDR_PHY_DVFS_MODE0:
        default:
            selected_offset = entries[i].off_mode0;
            break;
        }

        if (selected_offset == RK3576_DDR_DVFS_INVALID_OFFSET) {
            entries[i].value = 0;
        } else {
            entries[i].value = phy_words[selected_offset / sizeof(uint32_t)];
            if (actual_mhz > RK3576_DDR_DVFS_HIGH_FREQ_THRESHOLD_MHZ &&
                (selected_offset & RK3576_DDR_DVFS_HIGH_FREQ_OFFSET_MASK) == RK3576_DDR_DVFS_HIGH_FREQ_MASKED_OFFSET)
                entries[i].value |= high_freq_rank_mask;
        }
    }
}

void rk3576_ddr_build_dvfs_fsp_record(rk3576_ddr_dvfs_fsp_record *record, uint32_t actual_mhz,
                                      const uint32_t phy_ch0_words[RK3576_DDR_PHY_WORD_COUNT],
                                      const uint32_t cru_words[RK3576_DDR_CRU_WORD_COUNT],
                                      const uint32_t build_context_words[RK3576_DDR_BUILD_CONTEXT_WORD_COUNT],
                                      rk3576_ddr_phy_dvfs_mode phy_dvfs_mode,
                                      const uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS])
{
    uint32_t cru_base = (cru_words[RK3576_DDR_CRU_PLL_ALT_SELECT_WORD] & 1u) ? RK3576_DDR_CRU_ALT_BASE_WORD : 0u;

    memset(record, 0, sizeof(*record));
    record->actual_mhz = actual_mhz;
    rk3576_ddr_build_dvfs_reg_values(record->phy_reg_entries, phy_ch0_words, phy_dvfs_mode, actual_mhz,
                                     build_context_words[RK3576_DDR_CONTEXT_CHANNEL0_RANK_COUNT]);
    record->sentinel_minus1 = RK3576_DDR_DVFS_INVALID_OFFSET;

    for (uint32_t i = 0; i < RK3576_DDR_CRU_PLL_WORDS; ++i)
        record->cru_pll_regs[i] = cru_words[cru_base + i];

    for (uint32_t slot = 0; slot < RK3576_DDR_MAX_CHANNELS; ++slot) {
        rk3576_ddr_channel_id channel = active_channel_map[slot];

        if (!rk3576_ddr_channel_is_active(channel))
            continue;
        record->channel_mode_register_packed[channel] =
            ((build_context_words[channel + RK3576_DDR_CONTEXT_MR_HIGH_BASE] & 0xffu) << 8) |
            (build_context_words[channel + RK3576_DDR_CONTEXT_MR_LOW_BASE] & 0xffu);
    }

    record->phy_ctrl_bit22_snapshot = phy_ch0_words[1] & RK3576_DDR_PHY_CTRL_BIT22;
    record->timing_word_147 = build_context_words[RK3576_DDR_CONTEXT_TIMING_WORD_147];
    for (uint32_t i = 0; i < RK3576_DDR_CONTEXT_TIMING_125_COUNT; ++i)
        record->timing_words_125_128[i] = build_context_words[RK3576_DDR_CONTEXT_TIMING_125_BASE + i];
    for (uint32_t i = 0; i < RK3576_DDR_CONTEXT_MR_SHADOW_COUNT; ++i)
        record->mr_shadow_words[i] = build_context_words[RK3576_DDR_CONTEXT_MR_SHADOW_BASE + i];
    record->magic = RK3576_DVFS_FSP_MAGIC;
}
