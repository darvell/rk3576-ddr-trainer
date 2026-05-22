#include "rk3576_board.h"
#include "rk3576_mmio.h"
#include "rk3576_payload.h"

#include "rk3576_ddr/boot.h"
#include "rk3576_ddr/debug_uart.h"
#include "rk3576_ddr/geometry.h"
#include "rk3576_ddr/tags.h"

#include <string.h>

#define RK3576_DDR_MAX_BOOT_STEPS 16u
#define RK3576_DDR_MAX_UART_STEPS 16u
#define RK3576_DDR_CRU_DEBUG_ROUTE_WORD_OFFSET 0x03a0u

__attribute__((aligned(16))) static uint8_t rk3576_ddr_config_tag_area[RK3576_DDR_CONFIG_TAG_AREA_SIZE];
static rk3576_ddr_geometry_registers rk3576_ddr_last_geometry;
static volatile uintptr_t rk3576_ddr_last_handoff_arg;

static void copy_to_physical(uintptr_t dst, const void *src, size_t len)
{
    memcpy((void *)dst, src, len);
}

static void apply_handoff_gates(const uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS])
{
    rk3576_ddr_handoff_gate_step steps[RK3576_DDR_MAX_CHANNELS];
    size_t count = rk3576_ddr_build_handoff_gate_steps(steps, RK3576_DDR_MAX_CHANNELS,
                                                       active_channel_map);

    for (size_t i = 0; i < count; ++i) {
        const rk3576_ddr_handoff_gate_step *step = &steps[i];
        uintptr_t ctl_base = step->channel ? RK3576_DDRCTL1_BASE : RK3576_DDRCTL0_BASE;
        uint32_t value;

        rk3576_ddr_raw_write32(RK3576_DDR_GRF_BASE + step->ddr_grf_gate_offset,
                               step->ddr_grf_gate_value);
        value = rk3576_ddr_raw_read32(ctl_base + step->pctl_offset);
        value &= ~step->pctl_clear_mask;
        rk3576_ddr_raw_write32(ctl_base + step->pctl_offset, value);
        rk3576_ddr_raw_write32(RK3576_DDR_GRF_BASE + step->ddr_grf_sideband_offset,
                               step->ddr_grf_sideband_value);
    }
}

static int boot_jump_requested(void)
{
    uint32_t request = rk3576_ddr_raw_read32(RK3576_DDR_PMU0_SGRF_BASE + 0x44u);

    if (request != RK3576_DDR_BOOT_REQUEST_MAGIC)
        return 0;
    rk3576_ddr_raw_write32(RK3576_DDR_PMU0_SGRF_BASE + 0x44u, RK3576_DDR_BOOT_REQUEST_ACK);
    return 1;
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
    int jump_requested;

    rk3576_ddr_board_config_init(&config);
    if (boot_arg)
        memcpy(&config.boot, boot_arg, sizeof(config.boot));
    jump_requested = boot_jump_requested();

    boot_step_count = rk3576_ddr_build_early_clock_reset_steps(boot_steps, RK3576_DDR_MAX_BOOT_STEPS);
    rk3576_ddr_apply_register_steps(boot_steps, boot_step_count);

    uart_base = configure_debug_uart(config.debug_clock_hz);
    rk3576_ddr_raw_uart_puts(uart_base, "RK3576 DDR trainer\n");

    rk3576_ddr_build_geometry_registers(&rk3576_ddr_last_geometry, &config.geometry,
                                        config.active_channel_map, config.mr8_by_channel,
                                        &config.addrmap);
    rk3576_ddr_emit_config_tags(rk3576_ddr_config_tag_area, &config.geometry,
                                config.active_channel_map, config.sys_sgrf_word0, &config.boot);
    copy_to_physical(config.config_tags_addr, rk3576_ddr_config_tag_area,
                     sizeof(rk3576_ddr_config_tag_area));

    apply_handoff_gates(config.active_channel_map);
    handoff_arg = rk3576_ddr_copy_export_image((void *)config.export_image_addr,
                                               rk3576_ddr_export_payload_template,
                                               RK3576_DDR_EXPORT_PAYLOAD_LEN,
                                               config.geometry.dram_type);
    rk3576_ddr_last_handoff_arg = (uintptr_t)handoff_arg;

    rk3576_ddr_raw_uart_puts(uart_base, "RK3576 DDR trainer handoff\n");
    if (jump_requested)
        rk3576_ddr_platform_jump(config.boot_jump_addr);

    return RK3576_DDR_STATUS_OK;
}
