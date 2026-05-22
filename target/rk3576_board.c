#include "rk3576_board.h"

#include <string.h>

void rk3576_ddr_board_config_init(rk3576_ddr_board_config *config)
{
    rk3576_ddr_channel_geometry *channel0;
    rk3576_ddr_channel_geometry *channel1;

    memset(config, 0, sizeof(*config));

    config->geometry.dram_type = RK3576_LPDDR5;
    channel0 = &config->geometry.channel[0];
    channel1 = &config->geometry.channel[1];

    channel0->cs_count = 1u;
    channel0->col_bits = 10u;
    channel0->bank_bits_log2 = 4u;
    channel0->bus_width_code = 1u;
    channel0->die_bus_width_code = 1u;
    channel0->cs0_row_bits = 16u;
    channel0->cs0_alt_row_bits = 16u;

    *channel1 = *channel0;

    config->active_channel_map[0] = 0u;
    config->active_channel_map[1] = 1u;
    config->mr8_by_channel[0] = 0x5du;
    config->mr8_by_channel[1] = 0x5du;

    config->addrmap.column_address_bits = 10u;
    config->addrmap.rank_mode_code = 1u;
    config->debug_clock_hz = RK3576_DDR_UART_CLOCK_HZ;
    config->config_tags_addr = RK3576_DDR_CONFIG_TAGS_ADDR;
    config->dvfs_records_addr = RK3576_DDR_DVFS_RECORDS_ADDR;
    config->export_image_addr = RK3576_DDR_EXPORT_IMAGE_ADDR;
    config->boot_jump_addr = RK3576_DDR_BOOT_JUMP_ADDR;
}
