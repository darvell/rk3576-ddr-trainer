#include "rk3576_ddr/boot.h"

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

enum {
    RK3576_DDR_WARM_MAGIC = 0xef08a53cu,
    RK3576_DDR_BOOT_STATE_WARM_RESTART = 10u,
    RK3576_DDR_BOOT_STATE_NORMAL_MAX = 0x80u,
    RK3576_DDR_TIMER_SELECT_MASK = 0x00000f00u,
    RK3576_DDR_TIMER_24MHZ = 24000000ull,
    RK3576_DDR_TIMER_1GHZ = 1000000000ull,
    RK3576_DDR_OTP_GATE_MAGIC = 0x76354b52u,
    RK3576_DDR_OTP_GATE_WDT_ID = 0x113u,
    RK3576_DDR_OTP_INT_READY_MASK = 3u,
    RK3576_DDR_OTP_ID_MASK = 0xffffu,
    RK3576_DDR_OTP_ALLOWED_ID_FIRST = 0x101u,
    RK3576_DDR_OTP_ALLOWED_ID_RANGE = 0x0cu,
    RK3576_DDR_OTP_ALLOWED_ID_BITMAP = 0x1201u,
    RK3576_DDR_SYSSGRF_GATE_WORD300 = 0x80078007u,
    RK3576_DDR_WDT_TORR_VALUE = 0x0000000cu,
    RK3576_DDR_WDT_CR_VALUE = 0x00000009u,
    RK3576_DDR_WDT_CRR_RESTART = 0x00000076u,
    RK3576_DDR_SYSSGRF_SOC_CON1_OFFSET = 0x0004u,
    RK3576_DDR_SYSSGRF_CLOCK_GATE_OFFSET = 0x0140u,
    RK3576_DDR_SYSSGRF_DDRPHY_RESET_OFFSET = 0x0a80u,
    RK3576_DDR_SYSSGRF_DDRPHY_STATUS_OFFSET = 0x0ab0u,
    RK3576_DDR_CCI_GRF_DDR_OFFSET = 0x000cu,
    RK3576_DDR_PMU0_SGRF_DDR_OFFSET = 0x0004u,
    RK3576_DDR_PMU1_GRF_TIMER_OFFSET = 0x001cu,
};

typedef struct rk3576_ddr_early_register_step {
    uint32_t base;
    uint32_t offset;
    uint32_t mask;
    uint32_t value;
    rk3576_ddr_register_op op;
} rk3576_ddr_early_register_step;

static const rk3576_ddr_early_register_step rk3576_ddr_early_clock_reset_steps[] = {
    {RK3576_DDR_FW_SYSSGRF_BASE, RK3576_DDR_SYSSGRF_SOC_CON1_OFFSET, 0x00007000u, 0x00007000u, RK3576_DDR_REG_SET_BITS},
    {RK3576_DDR_FW_SYSSGRF_BASE, RK3576_DDR_SYSSGRF_CLOCK_GATE_OFFSET, RK3576_DDR_REG_FULL_MASK, 0xffff3fffu, RK3576_DDR_REG_WRITE},
    {RK3576_DDR_FW_SYSSGRF_BASE, RK3576_DDR_SYSSGRF_DDRPHY_RESET_OFFSET, RK3576_DDR_REG_FULL_MASK, 0x00380038u, RK3576_DDR_REG_WRITE},
    {RK3576_DDR_FW_SYSSGRF_BASE, RK3576_DDR_SYSSGRF_DDRPHY_STATUS_OFFSET, 0x00000038u, 0x00000038u, RK3576_DDR_REG_POLL_MASK_EQ},
    {RK3576_DDR_FW_SYSSGRF_BASE, RK3576_DDR_SYSSGRF_DDRPHY_RESET_OFFSET, RK3576_DDR_REG_FULL_MASK, 0x00380000u, RK3576_DDR_REG_WRITE},
    {RK3576_DDR_CCI_GRF_BASE, RK3576_DDR_CCI_GRF_DDR_OFFSET, RK3576_DDR_REG_FULL_MASK, 0x07000700u, RK3576_DDR_REG_WRITE},
    {RK3576_DDR_PMU0_SGRF_BASE, RK3576_DDR_PMU0_SGRF_DDR_OFFSET, RK3576_DDR_REG_FULL_MASK, 0x00040000u, RK3576_DDR_REG_WRITE},
    {RK3576_DDR_PMU1_GRF_BASE, RK3576_DDR_PMU1_GRF_TIMER_OFFSET, RK3576_DDR_REG_FULL_MASK, RK3576_DDR_TIMER_24MHZ, RK3576_DDR_REG_WRITE},
};

int rk3576_ddr_sram_boot_state_is_normal(uint32_t sram_boot_state_word)
{
    return sram_boot_state_word != RK3576_DDR_BOOT_STATE_WARM_RESTART &&
           sram_boot_state_word <= RK3576_DDR_BOOT_STATE_NORMAL_MAX;
}

int rk3576_ddr_warm_reentry_rejects_init(uint32_t pmu0_grf_os_reg16, uint32_t sram_boot_state_word)
{
    return pmu0_grf_os_reg16 == RK3576_DDR_WARM_MAGIC &&
           rk3576_ddr_sram_boot_state_is_normal(sram_boot_state_word);
}

uint64_t rk3576_ddr_select_timer_hz(uint32_t boot_word8_timer_flags)
{
    return (boot_word8_timer_flags & RK3576_DDR_TIMER_SELECT_MASK) ? RK3576_DDR_TIMER_1GHZ : RK3576_DDR_TIMER_24MHZ;
}

size_t rk3576_ddr_build_early_clock_reset_steps(rk3576_ddr_register_step *steps, size_t capacity)
{
    size_t count = 0;

    for (size_t i = 0; i < ARRAY_LEN(rk3576_ddr_early_clock_reset_steps); ++i) {
        const rk3576_ddr_early_register_step *step = &rk3576_ddr_early_clock_reset_steps[i];
        rk3576_ddr_append_register_step(steps, capacity, &count, step->op,
                                        step->base + step->offset, step->mask, step->value);
    }
    return count;
}

static int otp_gate_id_is_accepted(uint32_t otp_magic, uint32_t otp_id)
{
    uint32_t id = otp_id & RK3576_DDR_OTP_ID_MASK;

    if (otp_magic == 0)
        return id == 0;
    if (otp_magic != RK3576_DDR_OTP_GATE_MAGIC)
        return 0;
    if (id == RK3576_DDR_OTP_GATE_WDT_ID)
        return 1;
    if (id - RK3576_DDR_OTP_ALLOWED_ID_FIRST > RK3576_DDR_OTP_ALLOWED_ID_RANGE)
        return 0;
    return ((RK3576_DDR_OTP_ALLOWED_ID_BITMAP >> (id - RK3576_DDR_OTP_ALLOWED_ID_FIRST)) & 1u) != 0;
}

int rk3576_ddr_eval_otp_gate(rk3576_ddr_otp_gate_result *result, uint32_t otp_int_status,
                             uint32_t otp_magic, uint32_t otp_id, uint32_t sram_boot_state_word)
{
    uint32_t id = otp_id & RK3576_DDR_OTP_ID_MASK;

    if (result)
        *result = (rk3576_ddr_otp_gate_result){0};

    if ((otp_int_status & RK3576_DDR_OTP_INT_READY_MASK) != RK3576_DDR_OTP_INT_READY_MASK)
        return RK3576_DDR_STATUS_UNSUPPORTED;
    if (!otp_gate_id_is_accepted(otp_magic, id))
        return RK3576_DDR_STATUS_UNSUPPORTED;

    if (result) {
        result->accepted = 1u;
        result->writes_sys_sgrf_word300 = 1u;
        result->sys_sgrf_word300 = RK3576_DDR_SYSSGRF_GATE_WORD300;
        if (otp_magic == RK3576_DDR_OTP_GATE_MAGIC && id == RK3576_DDR_OTP_GATE_WDT_ID &&
            rk3576_ddr_sram_boot_state_is_normal(sram_boot_state_word)) {
            result->arms_secure_watchdog = 1u;
            result->wdt_torr = RK3576_DDR_WDT_TORR_VALUE;
            result->wdt_cr = RK3576_DDR_WDT_CR_VALUE;
            result->wdt_crr = RK3576_DDR_WDT_CRR_RESTART;
        }
    }
    return RK3576_DDR_STATUS_OK;
}
