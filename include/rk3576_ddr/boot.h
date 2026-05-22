#pragma once

#include <stdint.h>

#include <stddef.h>

#include "rk3576_ddr/abi.h"
#include "rk3576_ddr/register_plan.h"

typedef struct rk3576_ddr_otp_gate_result {
    uint32_t accepted;
    uint32_t writes_sys_sgrf_word300;
    uint32_t arms_secure_watchdog;
    uint32_t sys_sgrf_word300;
    uint32_t wdt_torr;
    uint32_t wdt_cr;
    uint32_t wdt_crr;
} rk3576_ddr_otp_gate_result;

int rk3576_ddr_sram_boot_state_is_normal(uint32_t sram_boot_state_word);
int rk3576_ddr_warm_reentry_rejects_init(uint32_t pmu0_grf_os_reg16, uint32_t sram_boot_state_word);
uint64_t rk3576_ddr_select_timer_hz(uint32_t boot_word8_timer_flags);
size_t rk3576_ddr_build_early_clock_reset_steps(rk3576_ddr_register_step *steps, size_t capacity);
int rk3576_ddr_eval_otp_gate(rk3576_ddr_otp_gate_result *result, uint32_t otp_int_status,
                             uint32_t otp_magic, uint32_t otp_id, uint32_t sram_boot_state_word);
