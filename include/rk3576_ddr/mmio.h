#pragma once

#include <stdint.h>

typedef struct rk3576_mmio_ops {
    uint32_t (*read32)(void *ctx, uintptr_t addr);
    void (*write32)(void *ctx, uintptr_t addr, uint32_t value);
    int (*poll32_mask_eq)(void *ctx, uintptr_t addr, uint32_t mask, uint32_t expected, uint32_t max_iters);
    void (*delay_us)(void *ctx, uint32_t usec);
    void (*uart_putc)(void *ctx, uint8_t ch);
    void (*handoff)(void *ctx, uintptr_t entry, const void *arg);
    void *ctx;
} rk3576_mmio_ops;
