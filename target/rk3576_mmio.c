#include "rk3576_mmio.h"

#define RK3576_DDR_UART_THR_OFFSET 0x00u
#define RK3576_DDR_UART_LSR_OFFSET 0x14u
#define RK3576_DDR_UART_LSR_THRE 0x20u
#define RK3576_DDR_DEFAULT_POLL_ITERS 1000000u
#define RK3576_DDR_DELAY_LOOP_ITERS_PER_US 32u

uint32_t rk3576_ddr_raw_read32(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

void rk3576_ddr_raw_write32(uintptr_t addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

int rk3576_ddr_raw_poll32_mask_eq(uintptr_t addr, uint32_t mask, uint32_t expected, uint32_t max_iters)
{
    for (uint32_t i = 0; i < max_iters; ++i) {
        if ((rk3576_ddr_raw_read32(addr) & mask) == expected)
            return RK3576_DDR_STATUS_OK;
    }
    return RK3576_DDR_STATUS_UNSUPPORTED;
}

void rk3576_ddr_raw_delay_us(uint32_t usec)
{
    volatile uint32_t sink = 0;

    for (uint32_t i = 0; i < usec; ++i) {
        for (uint32_t j = 0; j < RK3576_DDR_DELAY_LOOP_ITERS_PER_US; ++j)
            ++sink;
    }
}

void rk3576_ddr_raw_uart_putc(uintptr_t uart_base, uint8_t ch)
{
    if (!uart_base)
        return;
    (void)rk3576_ddr_raw_poll32_mask_eq(uart_base + RK3576_DDR_UART_LSR_OFFSET,
                                        RK3576_DDR_UART_LSR_THRE,
                                        RK3576_DDR_UART_LSR_THRE,
                                        RK3576_DDR_DEFAULT_POLL_ITERS);
    rk3576_ddr_raw_write32(uart_base + RK3576_DDR_UART_THR_OFFSET, ch);
}

void rk3576_ddr_raw_uart_puts(uintptr_t uart_base, const char *s)
{
    if (!s)
        return;
    while (*s) {
        if (*s == '\n')
            rk3576_ddr_raw_uart_putc(uart_base, '\r');
        rk3576_ddr_raw_uart_putc(uart_base, (uint8_t)*s++);
    }
}

void rk3576_ddr_apply_register_steps(const rk3576_ddr_register_step *steps, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        const rk3576_ddr_register_step *step = &steps[i];
        uint32_t value;

        switch (step->op) {
        case RK3576_DDR_REG_WRITE:
            rk3576_ddr_raw_write32(step->addr, step->value);
            break;
        case RK3576_DDR_REG_SET_BITS:
            value = rk3576_ddr_raw_read32(step->addr);
            value &= ~step->mask;
            value |= step->value;
            rk3576_ddr_raw_write32(step->addr, value);
            break;
        case RK3576_DDR_REG_CLEAR_BITS:
            value = rk3576_ddr_raw_read32(step->addr);
            value &= ~step->mask;
            rk3576_ddr_raw_write32(step->addr, value);
            break;
        case RK3576_DDR_REG_POLL_MASK_EQ:
            (void)rk3576_ddr_raw_poll32_mask_eq(step->addr, step->mask, step->value,
                                                RK3576_DDR_DEFAULT_POLL_ITERS);
            break;
        }
    }
}

void rk3576_ddr_platform_handoff(uintptr_t entry, const void *arg)
{
    void (*next_stage)(const void *) = (void (*)(const void *))entry;

    if (next_stage)
        next_stage(arg);
    for (;;)
        __asm__ volatile("wfe");
}

void rk3576_ddr_platform_jump(uintptr_t entry)
{
    void (*next_stage)(void) = (void (*)(void))entry;

    if (next_stage)
        next_stage();
    for (;;)
        __asm__ volatile("wfe");
}
