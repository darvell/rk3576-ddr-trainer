#include "rk3576_board.h"
#include "rk3576_mmio.h"

#include "rk3576_ddr/boot.h"
#include "rk3576_ddr/debug_uart.h"
#include "rk3576_ddr/geometry.h"
#include "rk3576_ddr/tags.h"

#include <string.h>

#define RK3576_DDR_MAX_BOOT_STEPS 16u
#define RK3576_DDR_MAX_UART_STEPS 16u
#define RK3576_DDR_CRU_DEBUG_ROUTE_WORD_OFFSET 0x03a0u

__attribute__((aligned(16))) static uint8_t rk3576_ddr_config_tag_area[RK3576_DDR_CONFIG_TAG_AREA_SIZE];
__attribute__((aligned(16))) static uint8_t rk3576_ddr_export_payload[RK3576_DDR_EXPORT_PAYLOAD_LEN];
__attribute__((aligned(16))) static uint8_t rk3576_ddr_export_image[RK3576_DDR_EXPORT_TOTAL_LEN];
static rk3576_ddr_geometry_registers rk3576_ddr_last_geometry;
static volatile uintptr_t rk3576_ddr_last_handoff_arg;

static void initialize_export_payload(void)
{
    for (size_t i = 0; i < sizeof(rk3576_ddr_export_payload); ++i)
        rk3576_ddr_export_payload[i] = 0;
}

static uintptr_t configure_debug_uart(uint32_t debug_clock_hz)
{
    rk3576_ddr_register_step steps[RK3576_DDR_MAX_UART_STEPS];
    size_t step_count = 0;
    uint32_t baud = 0;
    uint32_t route_word = rk3576_ddr_raw_read32(RK3576_DDR_CRU_BASE + RK3576_DDR_CRU_DEBUG_ROUTE_WORD_OFFSET);
    uint32_t uart_base = rk3576_ddr_select_debug_uart_base(route_word, debug_clock_hz, steps,
                                                           RK3576_DDR_MAX_UART_STEPS, &step_count, &baud);

    rk3576_ddr_apply_register_steps(steps, step_count);
    step_count = rk3576_ddr_build_debug_uart_init_steps(steps, RK3576_DDR_MAX_UART_STEPS, uart_base, baud);
    rk3576_ddr_apply_register_steps(steps, step_count);
    return uart_base;
}

int rk3576_ddr_entry(const rk3576_ddr_boot_params *boot_arg)
{
    rk3576_ddr_board_config config;
    rk3576_ddr_register_step boot_steps[RK3576_DDR_MAX_BOOT_STEPS];
    size_t boot_step_count;
    uintptr_t uart_base;
    void *handoff_arg;

    rk3576_ddr_board_config_init(&config);
    if (boot_arg)
        memcpy(&config.boot, boot_arg, sizeof(config.boot));

    boot_step_count = rk3576_ddr_build_early_clock_reset_steps(boot_steps, RK3576_DDR_MAX_BOOT_STEPS);
    rk3576_ddr_apply_register_steps(boot_steps, boot_step_count);

    uart_base = configure_debug_uart(config.debug_clock_hz);
    rk3576_ddr_raw_uart_puts(uart_base, "RK3576 DDR trainer\n");

    rk3576_ddr_build_geometry_registers(&rk3576_ddr_last_geometry, &config.geometry,
                                        config.active_channel_map, config.mr8_by_channel,
                                        &config.addrmap);
    rk3576_ddr_emit_config_tags(rk3576_ddr_config_tag_area, &config.geometry,
                                config.active_channel_map, config.sys_sgrf_word0, &config.boot);

    initialize_export_payload();
    handoff_arg = rk3576_ddr_copy_export_image(rk3576_ddr_export_image, rk3576_ddr_export_payload,
                                               sizeof(rk3576_ddr_export_payload), config.geometry.dram_type);
    rk3576_ddr_last_handoff_arg = (uintptr_t)handoff_arg;

    rk3576_ddr_raw_uart_puts(uart_base, "RK3576 DDR trainer handoff\n");
    if (config.handoff_entry)
        rk3576_ddr_platform_handoff(config.handoff_entry, handoff_arg);

    return RK3576_DDR_STATUS_OK;
}
