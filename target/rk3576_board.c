#include "rk3576_board.h"

#include <string.h>

void rk3576_ddr_board_config_init(rk3576_ddr_board_config *config)
{
    rk3576_ddr_channel_geometry *channel0;

    memset(config, 0, sizeof(*config));

    config->geometry.dram_type = RK3576_LPDDR5;
    channel0 = &config->geometry.channel[0];
    channel0->cs_count = 1u;
    channel0->col_bits = 10u;
    channel0->bank_bits_log2 = 3u;
    channel0->bus_width_code = 4u;
    channel0->die_bus_width_code = 0u;
    channel0->cs0_row_bits = 16u;

    config->active_channel_map[0] = 0u;
    config->active_channel_map[1] = RK3576_DISABLED_CHANNEL;
    config->mr8_by_channel[0] = 0x5du;
    config->mr8_by_channel[1] = 0u;

    config->addrmap.column_address_bits = 10u;
    config->addrmap.rank_mode_code = 1u;
    config->debug_clock_hz = RK3576_DDR_UART_CLOCK_HZ;
}
