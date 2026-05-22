#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum rk3576_ddr_register_op {
    RK3576_DDR_REG_WRITE = 1u,
    RK3576_DDR_REG_SET_BITS = 2u,
    RK3576_DDR_REG_POLL_MASK_EQ = 3u,
    RK3576_DDR_REG_CLEAR_BITS = 4u,
} rk3576_ddr_register_op;

typedef struct rk3576_ddr_register_step {
    rk3576_ddr_register_op op;
    uint32_t addr;
    uint32_t mask;
    uint32_t value;
} rk3576_ddr_register_step;

#define RK3576_DDR_FW_SYSSGRF_BASE 0x26005000u
#define RK3576_DDR_CCI_GRF_BASE    0x26010000u
#define RK3576_DDR_PMU0_SGRF_BASE  0x26000000u
#define RK3576_DDR_PMU1_GRF_BASE   0x26026000u
#define RK3576_DDR_CRU_BASE        0x27200000u
#define RK3576_DDRCTL0_BASE        0x28000000u
#define RK3576_DDRCTL1_BASE        0x29000000u
#define RK3576_DDR_GRF_BASE        0x26012000u
#define RK3576_DDR_HWLP0_BASE      0x2a060000u
#define RK3576_DDR_HWLP1_BASE      0x2a070000u
#define RK3576_DDR_UART_BASE       0x2ad40000u
#define RK3576_DDR_UART_CLOCK_HZ   1500000u
#define RK3576_DDR_REG_FULL_MASK   0xffffffffu

void rk3576_ddr_append_register_step(rk3576_ddr_register_step *steps, size_t capacity, size_t *count,
                                      rk3576_ddr_register_op op, uint32_t addr,
                                      uint32_t mask, uint32_t value);
