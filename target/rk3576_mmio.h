#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rk3576_ddr/abi.h"
#include "rk3576_ddr/register_plan.h"

uint32_t rk3576_ddr_raw_read32(uintptr_t addr);
void rk3576_ddr_raw_write32(uintptr_t addr, uint32_t value);
int rk3576_ddr_raw_poll32_mask_eq(uintptr_t addr, uint32_t mask, uint32_t expected, uint32_t max_iters);
void rk3576_ddr_raw_delay_us(uint32_t usec);
void rk3576_ddr_raw_uart_putc(uintptr_t uart_base, uint8_t ch);
void rk3576_ddr_raw_uart_puts(uintptr_t uart_base, const char *s);
void rk3576_ddr_apply_register_steps(const rk3576_ddr_register_step *steps, size_t count);
void rk3576_ddr_platform_handoff(uintptr_t entry, const void *arg);
