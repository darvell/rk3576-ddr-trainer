#include "rk3576_ddr/register_plan.h"

void rk3576_ddr_append_register_step(rk3576_ddr_register_step *steps, size_t capacity, size_t *count,
                                      rk3576_ddr_register_op op, uint32_t addr,
                                      uint32_t mask, uint32_t value)
{
    if (steps && *count < capacity) {
        steps[*count].op = op;
        steps[*count].addr = addr;
        steps[*count].mask = mask;
        steps[*count].value = value;
    }
    ++*count;
}
