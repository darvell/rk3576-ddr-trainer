#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rk3576_ddr/register_plan.h"

typedef struct rk3576_ddr_debug_arg {
    uint32_t word;
    const char *str;
} rk3576_ddr_debug_arg;

size_t rk3576_ddr_uart_crlf_transform(char *dst, size_t capacity, const char *src);
size_t rk3576_ddr_debug_format(char *dst, size_t capacity, const char *fmt, const rk3576_ddr_debug_arg *args, size_t arg_count);
uint32_t rk3576_ddr_select_debug_uart_base(uint32_t cru_word_3a0, uint32_t debug_clock_hz,
                                           rk3576_ddr_register_step *steps, size_t capacity,
                                           size_t *step_count_out, uint32_t *baud_out);
size_t rk3576_ddr_build_debug_uart_init_steps(rk3576_ddr_register_step *steps, size_t capacity,
                                              uint32_t uart_base, uint32_t baud_hz);
