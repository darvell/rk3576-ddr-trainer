#include "rk3576_ddr/geometry.h"

#include "rk3576_ddr/tags.h"

#include <string.h>

enum {
    RK3576_DDR_INACTIVE_CHANNEL_WORD48_MASK = 0xffff8000u,
    RK3576_DDR_ADDRMAP0_KEEP_COLUMN_MASK = 0xffff1fffu,
    RK3576_DDR_ADDRMAP1_KEEP_COLUMN_MASK = 0xffffcfffU,
    RK3576_DDR_ADDRMAP1_FIXED_HIGH_BITS = 0x30000000u,
    RK3576_DDR_ADDRMAP0_COL_LOW_MASK = 0x7u,
    RK3576_DDR_ADDRMAP0_COL_LOW_SHIFT = 13u,
    RK3576_DDR_ADDRMAP1_COL_HIGH_INPUT_SHIFT = 3u,
    RK3576_DDR_ADDRMAP1_COL_HIGH_MASK = 0x3u,
    RK3576_DDR_ADDRMAP1_COL_HIGH_SHIFT = 12u,
    RK3576_DDR_ADDRMAP_RANK_MODE_MAX = 2u,
    RK3576_DDR_ADDRMAP_RANK_MODE_BIAS = 1u,
    RK3576_DDR_ADDRMAP_RANK_MODE_SHIFT = 12u,
    RK3576_DDR_CHANNEL_PAIR_SIZE = 2u,
    RK3576_DDR_LOCAL_CHANNEL_MASK = 1u,
    RK3576_DDR_DIE_WIDTH_DIVIDEND = 2u,
    RK3576_DDR_ADDRMAP_DERATE_FLAG_BASE_SHIFT = 30u,
    RK3576_DDR_ADDRMAP_CHANNEL_PRESENT_BASE_SHIFT = 28u,
    RK3576_DDR_ADDRMAP1_CS2_DIFFERS_FLAG = 0x8000u,
    RK3576_DDR_ADDRMAP1_CS3_DIFFERS_FLAG = 0x10000u,
    RK3576_DDR_ADDRMAP0_CS0_ROW_SHIFT = 6u,
    RK3576_DDR_ADDRMAP0_CS1_ROW_SHIFT = 4u,
    RK3576_DDR_ADDRMAP1_CS0_ROW_SHIFT = 5u,
    RK3576_DDR_ADDRMAP1_CS1_ROW_SHIFT = 4u,
    RK3576_DDR_ADDRMAP0_DUAL_CS_SHIFT = 11u,
    RK3576_DDR_ADDRMAP1_QUAD_CS_SHIFT = 14u,
    RK3576_DDR_ADDRMAP0_DUAL_CS_CLEAR_MASK = 0xfffff7ffu,
    RK3576_DDR_ADDRMAP1_QUAD_CS_CLEAR_MASK = 0xffffbfffu,
    RK3576_DDR_CS_COUNT_DUAL = 2u,
    RK3576_DDR_CS_COUNT_QUAD = 4u,
    RK3576_DDR_ROW_BITS_BASE = 13u,
    RK3576_DDR_COL_BITS_BASE = 9u,
    RK3576_DDR_PMU0_TYPE_SHIFT = 27u,
    RK3576_DDR_MR8_BYTE_SHIFT = 8u,
    RK3576_DDR_DIE_WIDTH_SHIFT_BASE = 12u,
    RK3576_DDR_CS1_LARGER_SHIFT_BASE = 8u,
    RK3576_DDR_CS0_LARGER_SHIFT_BASE = 4u,
    RK3576_DDR_ADDRMAP_CHANNEL_STRIDE = 16u,
    RK3576_DDR_WCK_DDRCTL_FLAG = 1u,
    RK3576_DDR_WCK_PHY_FLAG_SHIFT_IN = 15u,
    RK3576_DDR_WCK_PHY_FLAG_SHIFT_OUT = 1u,
    RK3576_DDR_HANDOFF_GATE_BASE_INDEX = 332u,
    RK3576_DDR_HANDOFF_GATE_INDEX_MASK = 0x3ffu,
    RK3576_DDR_HANDOFF_GATE_STRIDE = 4u,
    RK3576_DDR_HANDOFF_GATE_VALUE = 0x40004000u,
    RK3576_DDR_HANDOFF_PCTL_OFFSET = 0x1018cu,
    RK3576_DDR_HANDOFF_PCTL_CLEAR_MASK = 0x3fu,
    RK3576_DDR_HANDOFF_SIDEBAND_VALUE = 0x00200020u,
    RK3576_DDR_HANDOFF_SIDEBAND_BASE = 4u,
    RK3576_DDR_HANDOFF_SIDEBAND_CHANNEL_SHIFT = 8u,
};

static void pack_addrmap_words(const rk3576_ddr_channel_geometry *channel,
                               const rk3576_ddr_addrmap_options *options,
                               uint32_t *addrmap0, uint32_t *addrmap1,
                               rk3576_ddr_channel_id physical_channel)
{
    uint32_t rank_mode = options->rank_mode_code;
    uint32_t local_channel = physical_channel > RK3576_DDR_LOCAL_CHANNEL_MASK ? physical_channel - RK3576_DDR_CHANNEL_PAIR_SIZE : physical_channel;
    uint32_t channel_bit_shift = RK3576_DDR_ADDRMAP_CHANNEL_STRIDE * local_channel;

    *addrmap0 &= RK3576_DDR_ADDRMAP0_KEEP_COLUMN_MASK;
    *addrmap1 &= RK3576_DDR_ADDRMAP1_KEEP_COLUMN_MASK;
    *addrmap0 |= (options->column_address_bits & RK3576_DDR_ADDRMAP0_COL_LOW_MASK) << RK3576_DDR_ADDRMAP0_COL_LOW_SHIFT;
    *addrmap1 |= ((options->column_address_bits >> RK3576_DDR_ADDRMAP1_COL_HIGH_INPUT_SHIFT) & RK3576_DDR_ADDRMAP1_COL_HIGH_MASK) << RK3576_DDR_ADDRMAP1_COL_HIGH_SHIFT;
    if (rank_mode > RK3576_DDR_ADDRMAP_RANK_MODE_MAX)
        rank_mode = RK3576_DDR_ADDRMAP_RANK_MODE_MAX;
    *addrmap0 |= (rank_mode - RK3576_DDR_ADDRMAP_RANK_MODE_BIAS) << RK3576_DDR_ADDRMAP_RANK_MODE_SHIFT;

    *addrmap0 |= channel->three_quarter_size_derate << (local_channel + RK3576_DDR_ADDRMAP_DERATE_FLAG_BASE_SHIFT);
    *addrmap0 |= 1u << (local_channel + RK3576_DDR_ADDRMAP_CHANNEL_PRESENT_BASE_SHIFT);
    if (local_channel) {
        *addrmap0 |= (channel->cs_count - 1u) << 27;
    } else {
        *addrmap0 &= RK3576_DDR_ADDRMAP0_DUAL_CS_CLEAR_MASK;
        *addrmap1 &= RK3576_DDR_ADDRMAP1_QUAD_CS_CLEAR_MASK;
        *addrmap0 |= (channel->cs_count == RK3576_DDR_CS_COUNT_DUAL) << RK3576_DDR_ADDRMAP0_DUAL_CS_SHIFT;
        *addrmap1 |= (channel->cs_count == RK3576_DDR_CS_COUNT_QUAD) << RK3576_DDR_ADDRMAP1_QUAD_CS_SHIFT;
    }

    *addrmap0 |= (channel->col_bits - RK3576_DDR_COL_BITS_BASE) << (channel_bit_shift + 9u);
    *addrmap0 |= (channel->bank_bits_log2 != 3u) << (channel_bit_shift + 8u);
    *addrmap0 |= (RK3576_DDR_DIE_WIDTH_DIVIDEND >> channel->bus_width_code) << (channel_bit_shift + 2u);
    *addrmap0 &= ~(3u << (channel_bit_shift + RK3576_DDR_ADDRMAP0_CS0_ROW_SHIFT));
    *addrmap0 |= (RK3576_DDR_DIE_WIDTH_DIVIDEND >> channel->die_bus_width_code) << channel_bit_shift;
    *addrmap1 &= ~(1u << (2u * local_channel + RK3576_DDR_ADDRMAP1_CS0_ROW_SHIFT));
    *addrmap0 |= ((channel->cs0_row_bits - RK3576_DDR_ROW_BITS_BASE) & 3u) <<
                 (channel_bit_shift + RK3576_DDR_ADDRMAP0_CS0_ROW_SHIFT);
    *addrmap1 |= (((channel->cs0_row_bits - RK3576_DDR_ROW_BITS_BASE) >> 2) & 1u) <<
                 (2u * local_channel + RK3576_DDR_ADDRMAP1_CS0_ROW_SHIFT);

    if (channel->cs1_row_bits) {
        *addrmap0 &= ~(3u << (channel_bit_shift + RK3576_DDR_ADDRMAP0_CS1_ROW_SHIFT));
        *addrmap1 &= ~(1u << (2u * local_channel + RK3576_DDR_ADDRMAP1_CS1_ROW_SHIFT));
        *addrmap0 |= ((channel->cs1_row_bits - RK3576_DDR_ROW_BITS_BASE) & 3u) <<
                     (channel_bit_shift + RK3576_DDR_ADDRMAP0_CS1_ROW_SHIFT);
        *addrmap1 |= (((channel->cs1_row_bits - RK3576_DDR_ROW_BITS_BASE) >> 2) & 1u) <<
                     (2u * local_channel + RK3576_DDR_ADDRMAP1_CS1_ROW_SHIFT);
    }

    if (!local_channel && channel->cs_count > 2u) {
        if (channel->cs2_row_bits != channel->cs0_row_bits)
            *addrmap1 |= RK3576_DDR_ADDRMAP1_CS2_DIFFERS_FLAG;
        if (channel->cs3_row_bits != channel->cs0_row_bits)
            *addrmap1 |= RK3576_DDR_ADDRMAP1_CS3_DIFFERS_FLAG;
    }

    *addrmap1 |= ((channel->col_bits - RK3576_DDR_COL_BITS_BASE) << (2u * local_channel)) |
                 RK3576_DDR_ADDRMAP1_FIXED_HIGH_BITS;
}

void rk3576_ddr_build_geometry_registers(rk3576_ddr_geometry_registers *out,
                                         const rk3576_ddr_runtime_geometry *geometry,
                                         const uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS],
                                         const uint32_t mr8_by_channel[RK3576_DDR_MAX_CHANNELS],
                                         const rk3576_ddr_addrmap_options *addrmap_options)
{
    uint32_t addrmap0 = 0;
    uint32_t addrmap1 = 0;

    memset(out, 0, sizeof(*out));
    out->pmu0_grf_word48 = geometry->dram_type << RK3576_DDR_PMU0_TYPE_SHIFT;

    for (uint32_t slot = 0; slot < RK3576_DDR_MAX_CHANNELS; ++slot) {
        rk3576_ddr_channel_id channel = active_channel_map[slot];
        const rk3576_ddr_channel_geometry *ch;

        if (!rk3576_ddr_channel_is_active(channel)) {
            out->pmu0_grf_word48 |= RK3576_DDR_INACTIVE_CHANNEL_WORD48_MASK;
            continue;
        }

        ch = &geometry->channel[channel];
        out->pmu0_grf_word4c_mr8 |= mr8_by_channel[channel] << (RK3576_DDR_MR8_BYTE_SHIFT * channel);
        out->pmu0_grf_word48 |= (ch->cs_count - 1u) << channel;
        out->pmu0_grf_word48 |= ch->die_bus_width_code << (channel + RK3576_DDR_DIE_WIDTH_SHIFT_BASE);
        if (ch->cs1_row_bits == ch->cs0_row_bits + 1u)
            out->pmu0_grf_word48 |= 1u << (channel + RK3576_DDR_CS1_LARGER_SHIFT_BASE);
        if (ch->cs0_row_bits == ch->cs1_row_bits + 1u)
            out->pmu0_grf_word48 |= 1u << (channel + RK3576_DDR_CS0_LARGER_SHIFT_BASE);
    }

    for (uint32_t slot = 0; slot < RK3576_DDR_MAX_CHANNELS; ++slot) {
        rk3576_ddr_channel_id channel = active_channel_map[slot];
        const rk3576_ddr_channel_geometry *ch;

        if (!rk3576_ddr_channel_is_active(channel))
            continue;
        ch = &geometry->channel[channel];
        if (!ch->col_bits)
            continue;

        pack_addrmap_words(ch, addrmap_options, &addrmap0, &addrmap1, channel);
        out->pmu1_grf_addrmap_208 = addrmap0;
        out->pmu1_grf_addrmap_20c = addrmap1;
        out->pmu1_grf_addrmap_210 = 0;
        out->pmu1_grf_addrmap_214 = 0;
        out->cs_size_bytes[channel][0] = rk3576_ddr_channel_cs_size(ch, 0, geometry->dram_type);
        out->cs_size_bytes[channel][1] = rk3576_ddr_channel_cs_size(ch, 1, geometry->dram_type);
        out->zq_channel_mask |= 1u << channel;
        out->final_control_channel_mask |= 1u << channel;
    }
}

uint32_t rk3576_ddr_capture_lpddr5_wck_mode(uint32_t ddrctl_active_fsp_word, uint32_t ddrphy_cfg_word)
{
    return (ddrctl_active_fsp_word & RK3576_DDR_WCK_DDRCTL_FLAG) |
           (((ddrphy_cfg_word >> RK3576_DDR_WCK_PHY_FLAG_SHIFT_IN) & 1u) << RK3576_DDR_WCK_PHY_FLAG_SHIFT_OUT);
}

size_t rk3576_ddr_build_handoff_gate_steps(rk3576_ddr_handoff_gate_step *steps, size_t capacity,
                                           const uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS])
{
    size_t count = 0;

    for (uint32_t slot = 0; slot < RK3576_DDR_MAX_CHANNELS; ++slot) {
        rk3576_ddr_channel_id channel = active_channel_map[slot];

        if (!rk3576_ddr_channel_is_active(channel))
            continue;
        if (steps && count < capacity) {
            rk3576_ddr_handoff_gate_step *step = &steps[count];

            step->channel = channel;
            step->ddr_grf_gate_offset = RK3576_DDR_HANDOFF_GATE_STRIDE *
                                        (((channel & 1u) + RK3576_DDR_HANDOFF_GATE_BASE_INDEX) &
                                         RK3576_DDR_HANDOFF_GATE_INDEX_MASK);
            step->ddr_grf_gate_value = RK3576_DDR_HANDOFF_GATE_VALUE;
            step->pctl_offset = RK3576_DDR_HANDOFF_PCTL_OFFSET;
            step->pctl_clear_mask = RK3576_DDR_HANDOFF_PCTL_CLEAR_MASK;
            step->ddr_grf_sideband_offset = ((channel & 1u) << RK3576_DDR_HANDOFF_SIDEBAND_CHANNEL_SHIFT) +
                                            RK3576_DDR_HANDOFF_SIDEBAND_BASE;
            step->ddr_grf_sideband_value = RK3576_DDR_HANDOFF_SIDEBAND_VALUE;
        }
        ++count;
    }

    return count;
}
