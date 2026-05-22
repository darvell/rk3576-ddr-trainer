#pragma once

#include "rk3576_ddr/abi.h"

typedef uint32_t rk3576_ddr_channel_id;
typedef uint32_t rk3576_ddr_dram_type;
typedef uint32_t rk3576_ddr_fsp_index;

static inline int rk3576_ddr_channel_is_active(rk3576_ddr_channel_id channel)
{
    return channel <= 1u;
}

static inline int rk3576_ddr_type_uses_static_timing(rk3576_ddr_dram_type dram_type)
{
    return dram_type == RK3576_LPDDR4 || dram_type == RK3576_LPDDR4X || dram_type == RK3576_LPDDR5;
}
