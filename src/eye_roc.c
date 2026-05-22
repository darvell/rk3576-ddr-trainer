#include "rk3576_ddr/eye_roc.h"

#include <string.h>

enum {
    RK3576_DDR_DQ_COUNT_X8 = 8u,
    RK3576_DDR_DQ_COUNT_X16 = 16u,
    RK3576_DDR_DQ_COUNT_X32 = 32u,
    RK3576_DDR_DDRCTL_WINDOW_OFFSET = 0x10000u,
    RK3576_DDR_EYE_DDRCTL_CFG_OFFSET = 0x180u,
    RK3576_DDR_EYE_DDRCTL_STATUS_OFFSET = 0x014u,
    RK3576_DDR_EYE_DDRCTL_CLEAR_MASK = 0xfffbfd00u,
    RK3576_DDR_EYE_DDRCTL_POLL_MASK = 0x00000007u,
    RK3576_DDR_EYE_DDRCTL_POLL_VALUE = 1u,
    RK3576_DDR_EYE_GRF_018_OFFSET = 0x018u,
    RK3576_DDR_EYE_GRF_000_OFFSET = 0x000u,
    RK3576_DDR_EYE_GRF_004_OFFSET = 0x004u,
    RK3576_DDR_EYE_GRF_018_VALUE = 0x80000000u,
    RK3576_DDR_EYE_GRF_000_VALUE = 0x1f000000u,
    RK3576_DDR_EYE_GRF_004_VALUE = 0x90c60000u,
    RK3576_DDR_EYE_LANE_GATE_BASE_INDEX = 332u,
    RK3576_DDR_EYE_LANE_GATE_INDEX_MASK = 0x3ffu,
    RK3576_DDR_EYE_LANE_GATE_VALUE = 0x40000000u,
    RK3576_DDR_GRF_WORD_STRIDE = 4u,
    RK3576_DDR_GRF_CHANNEL_WINDOW_SHIFT = 8u,
    RK3576_DDR_HWLP_WORD3_OFFSET = 0x00cu,
    RK3576_DDR_EYE_RESTORE_GRF_018_MASK = 0xff7f0000u,
    RK3576_DDR_EYE_RESTORE_GRF_000_MASK = 0x1f000000u,
    RK3576_DDR_EYE_RESTORE_GRF_004_MASK = 0x90c60000u,
    RK3576_DDR_EYE_RESTORE_LANE_MASK = 0x40000000u,
    RK3576_DDR_ROC_DQ_KEEP_MASK = 0xfffffe07u,
    RK3576_DDR_ROC_DQ_P_SHIFT = 3u,
    RK3576_DDR_ROC_DQ_N_SHIFT = 6u,
    RK3576_DDR_ROC_DQS_KEEP_MASK = 0xfffffe4fu,
    RK3576_DDR_ROC_DQS_P_SHIFT = 4u,
    RK3576_DDR_ROC_DQS_N_SHIFT = 7u,
    RK3576_DDR_EYE_INVALID_ROW = RK3576_DDR_REG_FULL_MASK,
    RK3576_DDR_EYE_MISSING_DELAY_MIN = 1024,
    RK3576_DDR_EYE_MISSING_DELAY_MAX = -1024,
};

uint32_t rk3576_ddr_eye_dq_count(uint32_t dq_width_mode)
{
    if (dq_width_mode == RK3576_DDR_DQ_WIDTH_MODE_X8)
        return RK3576_DDR_DQ_COUNT_X8;
    if (dq_width_mode == RK3576_DDR_DQ_WIDTH_MODE_X16)
        return RK3576_DDR_DQ_COUNT_X16;
    return RK3576_DDR_DQ_COUNT_X32;
}

static uint32_t channel_ddrctl_base(rk3576_ddr_channel_id channel)
{
    return channel & 1u ? RK3576_DDRCTL1_BASE : RK3576_DDRCTL0_BASE;
}

static uint32_t channel_hwlp_base(rk3576_ddr_channel_id channel)
{
    return channel & 1u ? RK3576_DDR_HWLP1_BASE : RK3576_DDR_HWLP0_BASE;
}

size_t rk3576_ddr_plan_eye_scan_override(rk3576_ddr_register_step *steps, size_t capacity,
                                         rk3576_ddr_channel_id channel, int restore,
                                         const rk3576_ddr_eye_override_saved *saved)
{
    size_t count = 0;
    uint32_t ch_sel = channel & 1u;
    uint32_t ddrctl_window = channel_ddrctl_base(channel) + RK3576_DDR_DDRCTL_WINDOW_OFFSET;
    uint32_t ddr_grf_ch = RK3576_DDR_GRF_BASE + (ch_sel << RK3576_DDR_GRF_CHANNEL_WINDOW_SHIFT);
    uint32_t hwlp = channel_hwlp_base(channel);
    uint32_t lane_gate = RK3576_DDR_GRF_BASE + RK3576_DDR_GRF_WORD_STRIDE * (((ch_sel + RK3576_DDR_EYE_LANE_GATE_BASE_INDEX) & RK3576_DDR_EYE_LANE_GATE_INDEX_MASK));

    if (!restore) {
        rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_CLEAR_BITS,
                                        ddrctl_window + RK3576_DDR_EYE_DDRCTL_CFG_OFFSET,
                                        RK3576_DDR_EYE_DDRCTL_CLEAR_MASK, 0);
        rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_POLL_MASK_EQ,
                                        ddrctl_window + RK3576_DDR_EYE_DDRCTL_STATUS_OFFSET,
                                        RK3576_DDR_EYE_DDRCTL_POLL_MASK, RK3576_DDR_EYE_DDRCTL_POLL_VALUE);
        rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE,
                                        ddr_grf_ch + RK3576_DDR_EYE_GRF_018_OFFSET, RK3576_DDR_REG_FULL_MASK,
                                        RK3576_DDR_EYE_GRF_018_VALUE);
        rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE,
                                        ddr_grf_ch + RK3576_DDR_EYE_GRF_000_OFFSET, RK3576_DDR_REG_FULL_MASK,
                                        RK3576_DDR_EYE_GRF_000_VALUE);
        rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE,
                                        ddr_grf_ch + RK3576_DDR_EYE_GRF_004_OFFSET, RK3576_DDR_REG_FULL_MASK,
                                        RK3576_DDR_EYE_GRF_004_VALUE);
        rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE,
                                        lane_gate, RK3576_DDR_REG_FULL_MASK, RK3576_DDR_EYE_LANE_GATE_VALUE);
        rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE, hwlp, RK3576_DDR_REG_FULL_MASK, 0);
        rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE, hwlp + RK3576_DDR_HWLP_WORD3_OFFSET, RK3576_DDR_REG_FULL_MASK, 0);
        return count;
    }

    if (!saved)
        return 0;
    rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE,
                                    ddr_grf_ch + RK3576_DDR_EYE_GRF_018_OFFSET, RK3576_DDR_REG_FULL_MASK,
                                    saved->ddr_grf_018_low_ff7f | RK3576_DDR_EYE_RESTORE_GRF_018_MASK);
    rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE,
                                    ddr_grf_ch + RK3576_DDR_EYE_GRF_000_OFFSET, RK3576_DDR_REG_FULL_MASK,
                                    saved->ddr_grf_000_low_1f00 | RK3576_DDR_EYE_RESTORE_GRF_000_MASK);
    rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE,
                                    ddr_grf_ch + RK3576_DDR_EYE_GRF_004_OFFSET, RK3576_DDR_REG_FULL_MASK,
                                    saved->ddr_grf_004_low_90c6 | RK3576_DDR_EYE_RESTORE_GRF_004_MASK);
    rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE,
                                    lane_gate, RK3576_DDR_REG_FULL_MASK, saved->ddr_grf_lane_low_4000 | RK3576_DDR_EYE_RESTORE_LANE_MASK);
    rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE,
                                    ddrctl_window + RK3576_DDR_EYE_DDRCTL_CFG_OFFSET, RK3576_DDR_REG_FULL_MASK, saved->ddrctl_10180);
    rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE, hwlp, RK3576_DDR_REG_FULL_MASK, saved->hwlp_word0);
    rk3576_ddr_append_register_step(steps, capacity, &count, RK3576_DDR_REG_WRITE, hwlp + RK3576_DDR_HWLP_WORD3_OFFSET, RK3576_DDR_REG_FULL_MASK, saved->hwlp_word3);
    return count;
}

void rk3576_ddr_select_dq_roc(rk3576_ddr_roc_choice best[RK3576_DDR_EYE_ROC_DQ_LANES],
                              const rk3576_ddr_dq_roc_candidate *candidates, size_t candidate_count)
{
    if (!best)
        return;
    memset(best, 0, RK3576_DDR_EYE_ROC_DQ_LANES * sizeof(best[0]));
    if (!candidates)
        return;
    for (size_t i = 0; i < candidate_count; ++i) {
        for (size_t lane = 0; lane < RK3576_DDR_EYE_ROC_DQ_LANES; ++lane) {
            if (candidates[i].width[lane] > best[lane].width) {
                best[lane].width = candidates[i].width[lane];
                best[lane].p_code = candidates[i].p_code;
                best[lane].n_code = candidates[i].n_code;
            }
        }
    }
}

void rk3576_ddr_select_dqs_roc(rk3576_ddr_roc_choice best[2],
                               const rk3576_ddr_dqs_roc_candidate *candidates, size_t candidate_count)
{
    if (!best)
        return;
    memset(best, 0, 2u * sizeof(best[0]));
    if (!candidates)
        return;
    for (size_t i = 0; i < candidate_count; ++i) {
        if (candidates[i].low_lane_min_width > best[0].width) {
            best[0].width = candidates[i].low_lane_min_width;
            best[0].p_code = candidates[i].p_code;
            best[0].n_code = candidates[i].n_code;
        }
        if (candidates[i].high_lane_min_width > best[1].width) {
            best[1].width = candidates[i].high_lane_min_width;
            best[1].p_code = candidates[i].p_code;
            best[1].n_code = candidates[i].n_code;
        }
    }
}

uint32_t rk3576_ddr_pack_dq_roc_word(uint32_t phy_word, const rk3576_ddr_roc_choice *best)
{
    if (!best)
        return phy_word;
    return (phy_word & RK3576_DDR_ROC_DQ_KEEP_MASK) |
           ((uint32_t)best->p_code << RK3576_DDR_ROC_DQ_P_SHIFT) |
           ((uint32_t)best->n_code << RK3576_DDR_ROC_DQ_N_SHIFT);
}

uint32_t rk3576_ddr_pack_dqs_roc_word(uint32_t phy_word, const rk3576_ddr_roc_choice *best)
{
    if (!best)
        return phy_word;
    return (phy_word & RK3576_DDR_ROC_DQS_KEEP_MASK) |
           ((uint32_t)best->p_code << RK3576_DDR_ROC_DQS_P_SHIFT) |
           ((uint32_t)best->n_code << RK3576_DDR_ROC_DQS_N_SHIFT);
}

int rk3576_ddr_summarize_eye_rows(rk3576_ddr_eye_window_summary *summary,
                                  const rk3576_ddr_eye_window_row *rows, size_t row_count,
                                  int32_t default_vref_x10, uint32_t min_eye_threshold_x10)
{
    int32_t best_width = -1;

    if (!summary)
        return RK3576_DDR_STATUS_UNSUPPORTED;
    memset(summary, 0, sizeof(*summary));
    summary->best_row = RK3576_DDR_EYE_INVALID_ROW;
    summary->current_row = RK3576_DDR_EYE_INVALID_ROW;

    if (!rows)
        return RK3576_DDR_STATUS_UNSUPPORTED;

    for (size_t i = 0; i < row_count && i < RK3576_DDR_EYE_MAX_ROWS; ++i) {
        int32_t width;

        if (!rows[i].vref_x10)
            break;
        if (rows[i].delay_min > rows[i].delay_max)
            continue;
        width = rows[i].delay_max - rows[i].delay_min;
        if (!summary->valid_rows)
            summary->first_vref_x10 = rows[i].vref_x10;
        if (width > best_width) {
            best_width = width;
            summary->best_row = (uint32_t)i;
            summary->best_vref_x10 = rows[i].vref_x10;
            summary->best_delay_min = rows[i].delay_min;
            summary->best_delay_max = rows[i].delay_max;
            summary->best_width = width;
        }
        if (summary->current_row == RK3576_DDR_EYE_INVALID_ROW) {
            int is_current = i + 1u == row_count || !rows[i + 1u].vref_x10 ||
                             (rows[i].vref_x10 >= default_vref_x10 && default_vref_x10 > rows[i + 1u].vref_x10);
            if (is_current) {
                summary->current_row = (uint32_t)i;
                summary->current_vref_x10 = rows[i].vref_x10;
                summary->current_delay_min = rows[i].delay_min;
                summary->current_delay_max = rows[i].delay_max;
            }
        }
        ++summary->valid_rows;
    }

    if (!summary->valid_rows)
        summary->fail_mask |= RK3576_DDR_EYE_FAIL_NO_ROWS;
    if (min_eye_threshold_x10 && (uint32_t)summary->first_vref_x10 < min_eye_threshold_x10)
        summary->fail_mask |= RK3576_DDR_EYE_FAIL_MIN_VREF;
    return summary->fail_mask ? RK3576_DDR_STATUS_UNSUPPORTED : RK3576_DDR_STATUS_OK;
}

static int eye_row_in_default_vref_range(const rk3576_ddr_eye_window_row *row, int32_t default_vref_x10, uint32_t vref_margin_x10)
{
    if (!vref_margin_x10)
        return 0;
    return row->vref_x10 >= default_vref_x10 - (int32_t)vref_margin_x10 &&
           row->vref_x10 <= default_vref_x10 + (int32_t)vref_margin_x10;
}

int rk3576_ddr_summarize_eye_report(rk3576_ddr_eye_window_summary *summary,
                                    const rk3576_ddr_eye_window_row *rows, size_t row_count,
                                    int32_t default_vref_x10,
                                    const rk3576_ddr_eye_report_limits *limits, uint32_t is_write)
{
    uint32_t margin_x10;
    int32_t left_limit;
    int32_t right_limit;

    if (!limits)
        return RK3576_DDR_STATUS_UNSUPPORTED;
    rk3576_ddr_summarize_eye_rows(summary, rows, row_count, default_vref_x10, limits->min_eye_threshold);
    if (!summary || !rows)
        return RK3576_DDR_STATUS_UNSUPPORTED;

    if (is_write) {
        margin_x10 = limits->wr_vref_margin;
        left_limit = -(int32_t)limits->wr_left_limit;
        right_limit = (int32_t)limits->rd_vref_margin;
    } else {
        margin_x10 = limits->wr_right_limit;
        left_limit = -(int32_t)limits->rd_right_limit;
        right_limit = (int32_t)limits->rd_left_limit;
    }

    for (size_t i = 0; i < row_count && i < RK3576_DDR_EYE_MAX_ROWS; ++i) {
        if (!rows[i].vref_x10)
            break;
        if (rows[i].delay_min > rows[i].delay_max)
            continue;
        if (eye_row_in_default_vref_range(&rows[i], default_vref_x10, margin_x10) &&
            (rows[i].delay_min > left_limit || rows[i].delay_max < right_limit))
            summary->fail_mask |= RK3576_DDR_EYE_FAIL_MARGIN;
    }

    return summary->fail_mask ? RK3576_DDR_STATUS_UNSUPPORTED : RK3576_DDR_STATUS_OK;
}

size_t rk3576_ddr_intersect_eye_rows(rk3576_ddr_eye_window_row all_rows[RK3576_DDR_EYE_MAX_ROWS],
                                     const rk3576_ddr_eye_window_row *rows, size_t row_count,
                                     int initialized)
{
    size_t valid_count = 0;
    size_t scan_index = 0;

    if (!all_rows || !rows)
        return 0;
    if (!initialized) {
        memset(all_rows, 0, RK3576_DDR_EYE_MAX_ROWS * sizeof(all_rows[0]));
        for (size_t i = 0; i < row_count && i < RK3576_DDR_EYE_MAX_ROWS; ++i) {
            all_rows[i] = rows[i];
            if (!rows[i].vref_x10)
                break;
            if (rows[i].delay_min <= rows[i].delay_max)
                ++valid_count;
        }
        return valid_count;
    }

    for (size_t out = 0; out < RK3576_DDR_EYE_MAX_ROWS; ++out) {
        int matched = 0;
        if (!all_rows[out].vref_x10)
            break;
        while (scan_index < row_count && scan_index < RK3576_DDR_EYE_MAX_ROWS && rows[scan_index].vref_x10) {
            if (all_rows[out].vref_x10 == rows[scan_index].vref_x10) {
                if (all_rows[out].delay_min < rows[scan_index].delay_min)
                    all_rows[out].delay_min = rows[scan_index].delay_min;
                if (all_rows[out].delay_max > rows[scan_index].delay_max)
                    all_rows[out].delay_max = rows[scan_index].delay_max;
                ++scan_index;
                matched = 1;
                break;
            }
            if (all_rows[out].vref_x10 > rows[scan_index].vref_x10)
                break;
            ++scan_index;
        }
        if (!matched) {
            all_rows[out].delay_min = RK3576_DDR_EYE_MISSING_DELAY_MIN;
            all_rows[out].delay_max = RK3576_DDR_EYE_MISSING_DELAY_MAX;
        }
        if (all_rows[out].delay_min <= all_rows[out].delay_max)
            ++valid_count;
    }

    return valid_count;
}

int rk3576_ddr_summarize_eye_metric(rk3576_ddr_eye_metric_summary *summary,
                                    const int32_t *values_x10, size_t count)
{
    int64_t sum = 0;

    if (!summary || !values_x10 || !count)
        return RK3576_DDR_STATUS_UNSUPPORTED;
    memset(summary, 0, sizeof(*summary));
    summary->count = (uint32_t)count;
    summary->min_value = values_x10[0];
    summary->max_value = values_x10[0];

    for (size_t i = 0; i < count; ++i) {
        int32_t value = values_x10[i];
        if (value < summary->min_value) {
            summary->min_value = value;
            summary->min_index = (uint32_t)i;
        }
        if (value > summary->max_value)
            summary->max_value = value;
        sum += value;
    }
    summary->avg_value_x10 = (int32_t)(sum / (int64_t)count);
    return RK3576_DDR_STATUS_OK;
}
