#include "rk3576_ddr/debug_uart.h"

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

enum {
    RK3576_DDR_UART_CLOCK_HIGH_DISABLE_BIT = 0x10000000u,
    RK3576_DDR_UART_CLOCK_ROUTE_MASK = 0x00ffffffu,
    RK3576_DDR_UART_CLOCK_ROUTE_SHIFT = 24u,
    RK3576_DDR_UART_CRU_ALT_BIT = 0x1000u,
    RK3576_DDR_UART_LCR_OFFSET = 0x0cu,
    RK3576_DDR_UART_RBR_THR_DLL_OFFSET = 0x00u,
    RK3576_DDR_UART_SFE_OFFSET = 0x98u,
    RK3576_DDR_UART_DLAB_8N1 = 0x83u,
    RK3576_DDR_UART_8N1 = 3u,
    RK3576_DDR_UART_FIFO_ENABLE = 1u,
    RK3576_DDR_UART_BAUD_115200 = 115200u,
    RK3576_DDR_UART_BAUD_750000 = 750000u,
    RK3576_DDR_UART_DIVISOR_115200 = 13u,
    RK3576_DDR_UART_DIVISOR_750000 = 2u,
    RK3576_DDR_UART_ROUTE_PMU0_IOC = 0u,
    RK3576_DDR_UART_ROUTE_PMU1_IOC = 1u,
    RK3576_DDR_PMU0_IOC_BASE = 0x26042000u,
    RK3576_DDR_PMU1_IOC_BASE = 0x26044000u,
    RK3576_DDR_IOC_MUX10_OFFSET = 0x10u,
    RK3576_DDR_IOC_MUX30_OFFSET = 0x30u,
    RK3576_DDR_IOC_MUX40_OFFSET = 0x40u,
    RK3576_DDR_CRU_UART_GATE_OFFSET = 0x0280u,
    RK3576_DDR_CRU_UART_PLL_CON0_OFFSET = 0x01c0u,
    RK3576_DDR_CRU_UART_PLL_CON1_OFFSET = 0x01c4u,
    RK3576_DDR_CRU_UART_PLL_CON2_OFFSET = 0x01c8u,
    RK3576_DDR_CRU_UART_CLKSEL_OFFSET = 0x0354u,
    RK3576_DDR_CRU_UART_CLKGATE_OFFSET = 0x0358u,
    RK3576_DDR_CRU_UART_ROUTE_OFFSET = 0x03f0u,
};

typedef struct rk3576_ddr_register_write_value {
    uint32_t offset;
    uint32_t value;
} rk3576_ddr_register_write_value;

static const rk3576_ddr_register_write_value rk3576_ddr_uart_alt_cru_steps[] = {
    {RK3576_DDR_CRU_UART_GATE_OFFSET, 0x000c0000u},
    {RK3576_DDR_CRU_UART_PLL_CON0_OFFSET, 0x03ff00b7u},
    {RK3576_DDR_CRU_UART_PLL_CON1_OFFSET, 0x01ff0042u},
    {RK3576_DDR_CRU_UART_PLL_CON2_OFFSET, 0xffffc4ecu},
    {RK3576_DDR_CRU_UART_GATE_OFFSET, 0x000c0004u},
    {RK3576_DDR_CRU_UART_CLKSEL_OFFSET, 0x006409a8u},
    {RK3576_DDR_CRU_UART_CLKGATE_OFFSET, 0x00030000u},
    {RK3576_DDR_CRU_UART_ROUTE_OFFSET, 0x07ff0401u},
};

static const rk3576_ddr_register_write_value rk3576_ddr_uart_main_cru_steps[] = {
    {RK3576_DDR_CRU_UART_ROUTE_OFFSET, 0x07ff0300u},
};

static const rk3576_ddr_register_write_value rk3576_ddr_uart_pmu0_ioc_steps[] = {
    {RK3576_DDR_IOC_MUX10_OFFSET, 0x000f0009u},
    {RK3576_DDR_IOC_MUX10_OFFSET, 0x00f00090u},
    {RK3576_DDR_IOC_MUX30_OFFSET, 0x0c000c00u},
};

static const rk3576_ddr_register_write_value rk3576_ddr_uart_pmu1_ioc_steps[] = {
    {RK3576_DDR_IOC_MUX40_OFFSET, 0x00f00090u},
    {RK3576_DDR_IOC_MUX40_OFFSET, 0x000f0009u},
};

static void append_cru_writes(rk3576_ddr_register_step *steps, size_t capacity, size_t *count,
                              const rk3576_ddr_register_write_value *writes, size_t write_count)
{
    for (size_t i = 0; i < write_count; ++i)
        rk3576_ddr_append_register_step(steps, capacity, count, RK3576_DDR_REG_WRITE,
                                        RK3576_DDR_CRU_BASE + writes[i].offset,
                                        RK3576_DDR_REG_FULL_MASK, writes[i].value);
}

static void append_ioc_writes(rk3576_ddr_register_step *steps, size_t capacity, size_t *count,
                              uint32_t base, const rk3576_ddr_register_write_value *writes,
                              size_t write_count)
{
    for (size_t i = 0; i < write_count; ++i)
        rk3576_ddr_append_register_step(steps, capacity, count, RK3576_DDR_REG_WRITE,
                                        base + writes[i].offset,
                                        RK3576_DDR_REG_FULL_MASK, writes[i].value);
}

static void append_char(char *dst, size_t capacity, size_t *count, char ch)
{
    if (dst && capacity && *count + 1u < capacity)
        dst[*count] = ch;
    ++*count;
    if (dst && capacity)
        dst[*count < capacity ? *count : capacity - 1u] = 0;
}

static void append_cstr(char *dst, size_t capacity, size_t *count, const char *src)
{
    if (!src)
        return;
    while (*src)
        append_char(dst, capacity, count, *src++);
}

static void append_uint_base(char *dst, size_t capacity, size_t *count, uint32_t value, uint32_t base)
{
    char tmp[16];
    size_t n = 0;

    do {
        uint32_t digit = value % base;
        tmp[n++] = (char)(digit < 10u ? '0' + digit : 'a' + digit - 10u);
        value /= base;
    } while (value);

    while (n)
        append_char(dst, capacity, count, tmp[--n]);
}

static void append_debug_decimal(char *dst, size_t capacity, size_t *count, uint32_t word)
{
    if (word & 0x80000000u) {
        append_char(dst, capacity, count, '-');
        word = (uint32_t)(-word);
    }
    append_uint_base(dst, capacity, count, word, 10u);
}

size_t rk3576_ddr_uart_crlf_transform(char *dst, size_t capacity, const char *src)
{
    size_t count = 0;

    if (dst && capacity)
        dst[0] = 0;
    if (!src)
        return 0;

    while (*src) {
        if (*src == '\n')
            append_char(dst, capacity, &count, '\r');
        append_char(dst, capacity, &count, *src++);
    }
    return count;
}

size_t rk3576_ddr_debug_format(char *dst, size_t capacity, const char *fmt, const rk3576_ddr_debug_arg *args, size_t arg_count)
{
    size_t count = 0;
    size_t arg = 0;

    if (dst && capacity)
        dst[0] = 0;
    if (!fmt)
        return 0;

    while (*fmt) {
        char spec;
        uint32_t word = 0;
        const char *str = 0;

        if (*fmt != '%') {
            append_char(dst, capacity, &count, *fmt++);
            continue;
        }

        ++fmt;
        spec = *fmt;
        if (!spec)
            break;
        ++fmt;

        if (spec == '%') {
            append_char(dst, capacity, &count, '%');
            continue;
        }

        if (arg < arg_count && args) {
            word = args[arg].word;
            str = args[arg].str;
        }

        switch (spec) {
        case 'd':
        case 'i':
        case 'u':
        case 'o':
            append_debug_decimal(dst, capacity, &count, word);
            ++arg;
            break;
        case 'x':
        case 'X':
            append_uint_base(dst, capacity, &count, word, 16u);
            ++arg;
            break;
        case 's':
            append_cstr(dst, capacity, &count, str);
            ++arg;
            break;
        case 'c':
            append_char(dst, capacity, &count, (char)word);
            ++arg;
            break;
        default:
            break;
        }
    }
    return count;
}

uint32_t rk3576_ddr_select_debug_uart_base(uint32_t cru_word_3a0, uint32_t debug_clock_hz,
                                           rk3576_ddr_register_step *steps, size_t capacity,
                                           size_t *step_count_out, uint32_t *baud_out)
{
    size_t count = 0;

    if (cru_word_3a0 & RK3576_DDR_UART_CRU_ALT_BIT)
        append_cru_writes(steps, capacity, &count, rk3576_ddr_uart_alt_cru_steps,
                          ARRAY_LEN(rk3576_ddr_uart_alt_cru_steps));
    else
        append_cru_writes(steps, capacity, &count, rk3576_ddr_uart_main_cru_steps,
                          ARRAY_LEN(rk3576_ddr_uart_main_cru_steps));

    if (baud_out)
        *baud_out = debug_clock_hz & RK3576_DDR_UART_CLOCK_ROUTE_MASK;
    if (debug_clock_hz & RK3576_DDR_UART_CLOCK_HIGH_DISABLE_BIT) {
        if (step_count_out)
            *step_count_out = count;
        return 0;
    }

    switch ((debug_clock_hz >> RK3576_DDR_UART_CLOCK_ROUTE_SHIFT) & 0xffu) {
    case RK3576_DDR_UART_ROUTE_PMU0_IOC:
        append_ioc_writes(steps, capacity, &count, RK3576_DDR_PMU0_IOC_BASE,
                          rk3576_ddr_uart_pmu0_ioc_steps,
                          ARRAY_LEN(rk3576_ddr_uart_pmu0_ioc_steps));
        break;
    case RK3576_DDR_UART_ROUTE_PMU1_IOC:
        append_ioc_writes(steps, capacity, &count, RK3576_DDR_PMU1_IOC_BASE,
                          rk3576_ddr_uart_pmu1_ioc_steps,
                          ARRAY_LEN(rk3576_ddr_uart_pmu1_ioc_steps));
        break;
    default:
        break;
    }

    if (step_count_out)
        *step_count_out = count;
    return RK3576_DDR_UART_BASE;
}

size_t rk3576_ddr_build_debug_uart_init_steps(rk3576_ddr_register_step *steps, size_t capacity,
                                              uint32_t uart_base, uint32_t baud_hz)
{
    size_t count = 0;
    uint32_t divisor = 1u;

    if (!uart_base)
        return 0;
    if (baud_hz == RK3576_DDR_UART_BAUD_115200)
        divisor = RK3576_DDR_UART_DIVISOR_115200;
    else if (baud_hz == RK3576_DDR_UART_BAUD_750000)
        divisor = RK3576_DDR_UART_DIVISOR_750000;

    rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE,
                                    uart_base + RK3576_DDR_UART_LCR_OFFSET, RK3576_DDR_REG_FULL_MASK,
                                    RK3576_DDR_UART_DLAB_8N1);
    rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE,
                                    uart_base + RK3576_DDR_UART_RBR_THR_DLL_OFFSET, RK3576_DDR_REG_FULL_MASK, divisor);
    rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE,
                                    uart_base + RK3576_DDR_UART_LCR_OFFSET, RK3576_DDR_REG_FULL_MASK,
                                    RK3576_DDR_UART_8N1);
    rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE,
                                    uart_base + RK3576_DDR_UART_SFE_OFFSET, RK3576_DDR_REG_FULL_MASK,
                                    RK3576_DDR_UART_FIFO_ENABLE);
    return count;
}
