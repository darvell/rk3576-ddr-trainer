#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rk3576_ddr/register_plan.h"
#include "rk3576_ddr/types.h"

#define RK3576_DDR_EYE_MAX_DQ_BITS 32u
#define RK3576_DDR_EYE_MAX_ROWS    20u
#define RK3576_DDR_EYE_ROC_DQ_LANES 18u

typedef enum rk3576_ddr_dq_width_mode {
    RK3576_DDR_DQ_WIDTH_MODE_X8 = 0u,
    RK3576_DDR_DQ_WIDTH_MODE_X16 = 1u,
    RK3576_DDR_DQ_WIDTH_MODE_X32 = 2u,
} rk3576_ddr_dq_width_mode;

typedef enum rk3576_ddr_eye_fail_flags {
    RK3576_DDR_EYE_FAIL_NO_ROWS = 1u,
    RK3576_DDR_EYE_FAIL_MIN_VREF = 2u,
    RK3576_DDR_EYE_FAIL_MARGIN = 4u,
} rk3576_ddr_eye_fail_flags;

typedef struct rk3576_ddr_eye_override_saved {
    uint32_t ddrctl_10180;
    uint32_t ddr_grf_018_low_ff7f;
    uint32_t ddr_grf_000_low_1f00;
    uint32_t ddr_grf_004_low_90c6;
    uint32_t ddr_grf_lane_low_4000;
    uint32_t hwlp_word0;
    uint32_t hwlp_word3;
} rk3576_ddr_eye_override_saved;

typedef struct rk3576_ddr_roc_choice {
    uint8_t p_code;
    uint8_t n_code;
    uint16_t width;
} rk3576_ddr_roc_choice;

typedef struct rk3576_ddr_dq_roc_candidate {
    uint8_t p_code;
    uint8_t n_code;
    uint16_t reserved;
    uint16_t width[RK3576_DDR_EYE_ROC_DQ_LANES];
} rk3576_ddr_dq_roc_candidate;

typedef struct rk3576_ddr_dqs_roc_candidate {
    uint8_t p_code;
    uint8_t n_code;
    uint16_t low_lane_min_width;
    uint16_t high_lane_min_width;
} rk3576_ddr_dqs_roc_candidate;

typedef struct rk3576_ddr_eye_window_row {
    int32_t vref_x10;
    int32_t delay_min;
    int32_t delay_max;
} rk3576_ddr_eye_window_row;

typedef struct rk3576_ddr_eye_window_summary {
    uint32_t valid_rows;
    uint32_t best_row;
    uint32_t current_row;
    int32_t best_vref_x10;
    int32_t best_delay_min;
    int32_t best_delay_max;
    int32_t best_width;
    int32_t current_vref_x10;
    int32_t current_delay_min;
    int32_t current_delay_max;
    int32_t first_vref_x10;
    uint32_t fail_mask;
} rk3576_ddr_eye_window_summary;

typedef struct rk3576_ddr_eye_report_limits {
    uint32_t rd_left_limit;
    uint32_t rd_right_limit;
    uint32_t rd_vref_margin;
    uint32_t wr_left_limit;
    uint32_t wr_right_limit;
    uint32_t wr_vref_margin;
    uint32_t min_eye_threshold;
} rk3576_ddr_eye_report_limits;

typedef struct rk3576_ddr_eye_metric_summary {
    uint32_t count;
    uint32_t min_index;
    int32_t min_value;
    int32_t max_value;
    int32_t avg_value_x10;
} rk3576_ddr_eye_metric_summary;

uint32_t rk3576_ddr_eye_dq_count(uint32_t dq_width_mode);
size_t rk3576_ddr_plan_eye_scan_override(rk3576_ddr_register_step *steps, size_t capacity,
                                         rk3576_ddr_channel_id channel, int restore,
                                         const rk3576_ddr_eye_override_saved *saved);
void rk3576_ddr_select_dq_roc(rk3576_ddr_roc_choice best[RK3576_DDR_EYE_ROC_DQ_LANES],
                              const rk3576_ddr_dq_roc_candidate *candidates, size_t candidate_count);
void rk3576_ddr_select_dqs_roc(rk3576_ddr_roc_choice best[2],
                               const rk3576_ddr_dqs_roc_candidate *candidates, size_t candidate_count);
uint32_t rk3576_ddr_pack_dq_roc_word(uint32_t phy_word, const rk3576_ddr_roc_choice *best);
uint32_t rk3576_ddr_pack_dqs_roc_word(uint32_t phy_word, const rk3576_ddr_roc_choice *best);
int rk3576_ddr_summarize_eye_rows(rk3576_ddr_eye_window_summary *summary,
                                  const rk3576_ddr_eye_window_row *rows, size_t row_count,
                                  int32_t default_vref_x10, uint32_t min_eye_threshold_x10);
int rk3576_ddr_summarize_eye_report(rk3576_ddr_eye_window_summary *summary,
                                    const rk3576_ddr_eye_window_row *rows, size_t row_count,
                                    int32_t default_vref_x10,
                                    const rk3576_ddr_eye_report_limits *limits, uint32_t is_write);
size_t rk3576_ddr_intersect_eye_rows(rk3576_ddr_eye_window_row all_rows[RK3576_DDR_EYE_MAX_ROWS],
                                     const rk3576_ddr_eye_window_row *rows, size_t row_count,
                                     int initialized);
int rk3576_ddr_summarize_eye_metric(rk3576_ddr_eye_metric_summary *summary,
                                    const int32_t *values_x10, size_t count);
