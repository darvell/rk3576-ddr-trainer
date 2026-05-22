#include "rk3576_ddr/boot.h"
#include "rk3576_ddr/debug_uart.h"
#include "rk3576_ddr/dvfs.h"
#include "rk3576_ddr/eye_roc.h"
#include "rk3576_ddr/geometry.h"
#include "rk3576_ddr/pctl_phy_plan.h"
#include "rk3576_ddr/tables.h"
#include "rk3576_ddr/tags.h"
#include "rk3576_ddr/timing.h"
#include "rk3576_ddr/training.h"

#include <assert.h>
#include <string.h>

static uint32_t load32(const void *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

int main(void)
{
    uint8_t tag_area[RK3576_DDR_CONFIG_TAG_AREA_SIZE] = {0};
    rk3576_ddr_boot_params boot = {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u, {0}};
    rk3576_ddr_ta58_boot_export ta58;
    rk3576_ddr_ta52_mem_regions ta52;
    rk3576_ddr_ta5a_static_info ta5a;
    uint8_t export_payload[RK3576_DDR_EXPORT_PAYLOAD_LEN];
    rk3576_ddr_runtime_geometry geometry;
    rk3576_ddr_addrmap_options addrmap_options;
    rk3576_ddr_geometry_registers geometry_regs;
    rk3576_ddr_handoff_gate_step handoff_steps[2];
    rk3576_ddr_dvfs_fsp_record dvfs_records[RK3576_DVFS_FSP_RECORD_COUNT];
    rk3576_ddr_dvfs_fsp_record fsp_record;
    rk3576_ddr_dvfs_channel_block dvfs_block;
    rk3576_ddr_dvfs_channel_tail dvfs_tail;
    uint32_t phy_words[RK3576_DDR_PHY_WORD_COUNT];
    uint32_t cru_words[RK3576_DDR_CRU_WORD_COUNT] = {0};
    uint32_t build_context_words[RK3576_DDR_BUILD_CONTEXT_WORD_COUNT] = {0};
    rk3576_ddr_reg_patch valid_patches[RK3576_PCTL_INIT_PATCH_ENTRY_COUNT];
    rk3576_ddr_training_pass training_passes[8];
    rk3576_ddr_pctl_phy_step pctl_phy_steps[24];
    rk3576_ddr_timing_seed timing_seed;
    rk3576_ddr_timing_descriptor timing_descriptor;
    rk3576_ddr_timing_values timing_values;
    rk3576_ddr_rate_step rate_steps[32];
    rk3576_ddr_register_step early_steps[8];
    rk3576_ddr_register_step debug_steps[16];
    rk3576_ddr_otp_gate_result otp_gate;
    rk3576_ddr_eye_override_saved eye_saved;
    rk3576_ddr_dq_roc_candidate dq_roc_candidates[3];
    rk3576_ddr_dqs_roc_candidate dqs_roc_candidates[3];
    rk3576_ddr_roc_choice roc_best[RK3576_DDR_EYE_ROC_DQ_LANES];
    rk3576_ddr_eye_window_row eye_rows[4];
    rk3576_ddr_eye_window_row eye_rows_narrow[4];
    rk3576_ddr_eye_window_row eye_all_rows[20];
    rk3576_ddr_eye_window_summary eye_summary;
    rk3576_ddr_eye_report_limits eye_limits;
    rk3576_ddr_eye_metric_summary eye_metric;
    int32_t eye_metric_values[4] = {450, 430, 480, 440};
    const rk3576_ddr_lp5_wck_entry *wck = 0;
    uint32_t active_map[2] = {0, RK3576_DISABLED_CHANNEL};
    uint32_t ch1_active_map[2] = {1, RK3576_DISABLED_CHANNEL};
    uint32_t dual_active_map[2] = {0, 1};
    uint32_t channel_rank_counts[2] = {1, 2};
    char fmt_out[64];
    char crlf_out[64];
    int low_table = 0;
    size_t debug_step_count = 0;
    uint32_t debug_baud = 0;
    uint32_t debug_base = 0;
    uint8_t export_with_header[RK3576_DDR_EXPORT_TOTAL_LEN];

    for (size_t i = 0; i < sizeof(export_payload); ++i)
        export_payload[i] = (uint8_t)i;

    assert(rk3576_ddr_tag_hash32(0, 0) == RK3576_TAG_HASH_SEED);
    assert(rk3576_ddr_tag_is_valid(0));
    assert(rk3576_ddr_tag_is_valid(RK3576_TAG_MAGIC_CONTAINER));
    assert(rk3576_ddr_tag_is_valid(RK3576_TAG_BOOT_EXPORT));
    assert(!rk3576_ddr_tag_is_valid(0x5441004fu));
    assert(rk3576_ddr_tag_is_valid(0x544100ffu));
    assert(!rk3576_ddr_tag_is_valid(0x54410100u));
    assert(rk3576_ddr_tag_record_word_count(RK3576_TAG_BOOT_EXPORT) == 0x10u);
    assert(rk3576_ddr_tag_payload_size(RK3576_TAG_BOOT_EXPORT) == sizeof(rk3576_ddr_ta58_boot_export));

    rk3576_ddr_build_ta58_boot_export(&ta58, &boot);
    assert(ta58.reserved_00 == 0);
    assert(ta58.boot_word0 == 0x11111111u);
    assert(ta58.boot_word4 == 0x22222222u);
    assert(ta58.boot_word8_timer_flags == 0x33333333u);
    assert(ta58.boot_wordc == 0x44444444u);

    assert(rk3576_ddr_tag_area_write(tag_area, RK3576_TAG_BOOT_EXPORT, &ta58) == 0);
    assert(load32(tag_area + 0x00) == 5u);
    assert(load32(tag_area + 0x04) == RK3576_TAG_MAGIC_CONTAINER);
    assert(load32(tag_area + 0x14) == 0x10u);
    assert(load32(tag_area + 0x18) == RK3576_TAG_BOOT_EXPORT);
    assert(load32(tag_area + 0x54) == 0u);
    assert(load32(tag_area + 0x58) == 0u);

    memset(&geometry, 0, sizeof(geometry));
    geometry.dram_type = RK3576_LPDDR5;
    geometry.channel[0].cs_count = 1;
    geometry.channel[0].col_bits = 10;
    geometry.channel[0].bank_bits_log2 = 3;
    geometry.channel[0].bus_width_code = 4;
    geometry.channel[0].cs0_row_bits = 16;
    geometry.channel[1].cs_count = 1;
    geometry.channel[1].col_bits = 10;
    geometry.channel[1].bank_bits_log2 = 3;
    geometry.channel[1].bus_width_code = 4;
    geometry.channel[1].cs0_row_bits = 15;

    assert(rk3576_ddr_channel_cs_size(&geometry.channel[0], 0, RK3576_LPDDR5) == (1ull << 33));
    assert(rk3576_ddr_channel_cs_size(&geometry.channel[1], 0, RK3576_LPDDR5) == (1ull << 32));
    rk3576_ddr_build_ta52_mem_regions(&ta52, &geometry, ch1_active_map, 0);
    assert(ta52.region_count == 1u);
    assert(ta52.regions[0].base == (1ull << 32));

    assert(rk3576_ddr_emit_config_tags(tag_area, &geometry, active_map, 0, &boot) == 0);
    assert(load32(tag_area + 0x14) == 0x0cu);
    assert(load32(tag_area + 0x18) == RK3576_TAG_BOOT_DEFAULTS);
    assert(load32(tag_area + 0x44) == 0x30u);
    assert(load32(tag_area + 0x48) == RK3576_TAG_MEM_REGION);
    assert(load32(tag_area + 0x104) == 0x10u);
    assert(load32(tag_area + 0x108) == RK3576_TAG_BOOT_EXPORT);
    assert(load32(tag_area + 0x144) == 0x4cu);
    assert(load32(tag_area + 0x148) == RK3576_TAG_STATIC_INFO);

    rk3576_ddr_build_ta5a_static_info(&ta5a, "clean-build");
    assert(strcmp(ta5a.build_id, "clean-build") == 0);

    assert(rk3576_ddr_select_phy_freq_row(RK3576_LPDDR4, 400) == &rk3576_ddr_lpddr4_phy_freq_rows[0]);
    assert(rk3576_ddr_select_phy_freq_row(RK3576_LPDDR4X, 614) == &rk3576_ddr_lpddr4_phy_freq_rows[2]);
    assert(rk3576_ddr_select_phy_freq_row(RK3576_LPDDR5, 2134) == &rk3576_ddr_lpddr5_phy_freq_rows[4]);
    assert(rk3576_ddr_select_phy_freq_row(RK3576_DDR3, 800) == 0);

    assert(rk3576_ddr_select_lpddr5_wck(1600, &wck, &low_table) == 0);
    assert(low_table == 1);
    assert(wck == &rk3576_ddr_lpddr5_wck_low_table[5]);
    assert(rk3576_ddr_pack_lpddr5_wck(wck) == ((4u << 24) | (8u << 28) | (0x14u << 8) | (0x24u << 16) | 0x38u));
    assert(rk3576_ddr_select_lpddr5_wck(3201, &wck, &low_table) == -1);

    assert(rk3576_ddr_copy_valid_pctl_init_patches(valid_patches, 3) == 33u);
    assert(valid_patches[0].register_offset == 0x00010200u);
    assert(valid_patches[0].clear_mask == 0x0000013fu);
    assert(valid_patches[0].set_value == 0x00000136u);
    assert(valid_patches[1].register_offset == 0x00020004u);
    assert(valid_patches[2].register_offset == 0x00021004u);
    assert(rk3576_ddr_pctl_init_patch_table[15].register_offset == 0xffffffffu);
    assert(rk3576_ddr_pctl_init_patch_table[52].register_offset == 0x00010380u);
    assert(rk3576_ddr_copy_valid_pctl_init_patches(0, 0) == 33u);

    timing_seed = rk3576_ddr_lpddr4_default_timing_seed;
    timing_seed.word[RK3576_DDR_TIMING_SEED_SPEED_BIN] = 21u;
    timing_seed.word[RK3576_DDR_TIMING_SEED_DRAM_TYPE] = RK3576_DDR3;
    timing_seed.word[RK3576_DDR_TIMING_SEED_TARGET_MHZ] = 400u;
    assert(rk3576_ddr_build_timing_descriptor(&timing_seed, &timing_descriptor, 0) == 40u);
    assert(timing_descriptor.word[RK3576_DDR_TIMING_DESC_SPEED_BIN] == 1u);
    timing_seed.word[RK3576_DDR_TIMING_SEED_TARGET_MHZ] = 401u;
    assert(rk3576_ddr_build_timing_descriptor(&timing_seed, &timing_descriptor, 0) == 40u);
    assert(timing_descriptor.word[RK3576_DDR_TIMING_DESC_SPEED_BIN] == 4u);
    timing_seed = rk3576_ddr_lpddr5_default_timing_seed;
    assert(rk3576_ddr_build_timing_descriptor(&timing_seed, &timing_descriptor, 0) == 40u);
    assert(timing_descriptor.word[RK3576_DDR_TIMING_DESC_SPEED_BIN] == 21u);
    assert(timing_descriptor.word[RK3576_DDR_TIMING_DESC_FILL_BASE] == RK3576_DDR_LPDDR5_TIMING_DEFAULT_OVERRIDE);
    assert(timing_descriptor.word[RK3576_DDR_TIMING_DESC_DRAM_TYPE] == RK3576_LPDDR5);
    assert(timing_descriptor.word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] == 800u);

    memset(&timing_descriptor, 0, sizeof(timing_descriptor));
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_DRAM_TYPE] = RK3576_LPDDR4;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] = 266u;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_DQ_BITS] = 32u;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_BUS_WIDTH] = 16u;
    assert(rk3576_ddr_derive_timing_values(&timing_descriptor, &timing_values, 4u) == RK3576_DDR_STATUS_OK);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_LPDDR4_WORD78] == 0u);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_B] == 4u);
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] = 267u;
    assert(rk3576_ddr_derive_timing_values(&timing_descriptor, &timing_values, 4u) == RK3576_DDR_STATUS_OK);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_LPDDR4_WORD78] == 9u);
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] = 533u;
    assert(rk3576_ddr_derive_timing_values(&timing_descriptor, &timing_values, 6u) == RK3576_DDR_STATUS_OK);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_LPDDR4_WORD78] == 9u);
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] = 534u;
    assert(rk3576_ddr_derive_timing_values(&timing_descriptor, &timing_values, 6u) == RK3576_DDR_STATUS_OK);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_LPDDR4_WORD78] == 18u);
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] = 1867u;
    assert(rk3576_ddr_derive_timing_values(&timing_descriptor, &timing_values, 0) == RK3576_DDR_STATUS_OK);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_LPDDR4_WORD78] == 63u);

    memset(&timing_descriptor, 0, sizeof(timing_descriptor));
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_DRAM_TYPE] = RK3576_LPDDR5;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] = 1600u;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_DQ_BITS] = 16u;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_LPDDR5_IO_WIDTH] = RK3576_DDR_LPDDR5_LOW_IO_WIDTH;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_LPDDR5_WRITE_LEVEL] = 3u;
    assert(rk3576_ddr_derive_timing_values(&timing_descriptor, &timing_values, 0) == RK3576_DDR_STATUS_OK);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_B] == rk3576_ddr_lpddr5_low_timing_profiles[5].word[RK3576_DDR_LPDDR5_PROFILE_CA_TRAIN_B]);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_A] == rk3576_ddr_lpddr5_low_timing_profiles[5].word[RK3576_DDR_LPDDR5_PROFILE_CA_TRAIN_A_MODE0]);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_LPDDR5_WORD138] == rk3576_ddr_lpddr5_low_timing_profiles[5].word[RK3576_DDR_LPDDR5_PROFILE_MODE_BASE]);
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] = 1601u;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_LPDDR5_IO_WIDTH] = 0u;
    assert(rk3576_ddr_derive_timing_values(&timing_descriptor, &timing_values, 0) == RK3576_DDR_STATUS_OK);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_B] == rk3576_ddr_lpddr5_high_timing_profiles[6].word[RK3576_DDR_LPDDR5_PROFILE_CA_TRAIN_B]);
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] = 2134u;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_LPDDR5_DBI] = 1u;
    assert(rk3576_ddr_derive_timing_values(&timing_descriptor, &timing_values, 0) == RK3576_DDR_STATUS_OK);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_B] == rk3576_ddr_lpddr5_high_dbi_timing_profiles[2].word[RK3576_DDR_LPDDR5_PROFILE_CA_TRAIN_B]);
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] = 4266u;
    assert(rk3576_ddr_derive_timing_values(&timing_descriptor, &timing_values, 0) == RK3576_DDR_STATUS_OK);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_B] == rk3576_ddr_lpddr5_high_dbi_timing_profiles[7].word[RK3576_DDR_LPDDR5_PROFILE_CA_TRAIN_B]);
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] = 4267u;
    assert(rk3576_ddr_derive_timing_values(&timing_descriptor, &timing_values, 0) == RK3576_DDR_STATUS_UNSUPPORTED);

    memset(&timing_descriptor, 0, sizeof(timing_descriptor));
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_DRAM_TYPE] = RK3576_LPDDR5;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_TARGET_MHZ] = 2134u;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_DQ_BITS] = 16u;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_BUS_WIDTH] = RK3576_DDR_TIMING_BUS_WIDTH_X8;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_ODT_PROFILE] = 1u;
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_LPDDR5_IO_WIDTH] = 0u;
    assert(rk3576_ddr_derive_timing_values(&timing_descriptor, &timing_values, 0) == RK3576_DDR_STATUS_OK);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_CA_TRAIN_A] == rk3576_ddr_lpddr5_high_timing_profiles[8].word[RK3576_DDR_LPDDR5_PROFILE_CA_TRAIN_A_MODE2]);
    assert(timing_values.word[RK3576_DDR_TIMING_VALUE_LPDDR5_WORD138] == rk3576_ddr_lpddr5_high_timing_profiles[8].word[RK3576_DDR_LPDDR5_PROFILE_MODE_VARIANT2]);
    timing_descriptor.word[RK3576_DDR_TIMING_DESC_DRAM_TYPE] = RK3576_DDR3;
    assert(rk3576_ddr_derive_timing_values(&timing_descriptor, &timing_values, 0) == RK3576_DDR_STATUS_UNSUPPORTED);

    assert(rk3576_ddr_plan_dvfs_training_passes(training_passes, 8, dual_active_map, channel_rank_counts, 7, 2) == 8u);
    assert(training_passes[0].channel == 0);
    assert(training_passes[0].rank_selector == 0);
    assert(training_passes[0].rank_slot == 7);
    assert(training_passes[0].fsp_index == 2);
    assert(training_passes[0].mode == RK3576_DDR_TRAINING_TTOT_MR12);
    assert(training_passes[0].ttot_enabled == 1);
    assert(training_passes[0].flags == RK3576_DDR_TRAINING_FLAG_ENTRY_SETUP);
    assert(training_passes[1].channel == 1);
    assert(training_passes[1].rank_selector == 3);
    assert(training_passes[2].mode == RK3576_DDR_TRAINING_TTOT_MR12);
    assert(training_passes[2].flags == RK3576_DDR_TRAINING_FLAG_RUN);
    assert(training_passes[4].mode == RK3576_DDR_TRAINING_DQ_PATTERN);
    assert(training_passes[4].flags == RK3576_DDR_TRAINING_FLAG_NONE);
    assert(training_passes[6].mode == RK3576_DDR_TRAINING_DQ_PATTERN);
    assert(training_passes[6].flags == (RK3576_DDR_TRAINING_FLAG_ENTRY_SETUP | RK3576_DDR_TRAINING_FLAG_EXIT_RESTORE | RK3576_DDR_TRAINING_FLAG_RUN));
    assert(rk3576_ddr_plan_dvfs_training_passes(0, 0, active_map, channel_rank_counts, 0, 0) == 4u);

    assert(rk3576_ddr_plan_pctl_phy_timing(pctl_phy_steps, 24, dual_active_map, RK3576_LPDDR5, 1, 0, 0, 0) == 18u);
    assert(pctl_phy_steps[0].phase == RK3576_DDR_PCTL_PHY_WRITE_PCTL_COMMON);
    assert(pctl_phy_steps[0].target == RK3576_DDR_PLAN_TARGET_DDRCTL);
    assert(pctl_phy_steps[0].channel == 0);
    assert(pctl_phy_steps[0].arg0 == RK3576_LPDDR5);
    assert(pctl_phy_steps[3].phase == RK3576_DDR_PCTL_PHY_CONFIGURE_LPDDR5);
    assert(pctl_phy_steps[3].offset == 0x10380u);
    assert(pctl_phy_steps[4].phase == RK3576_DDR_PCTL_PHY_APPLY_INIT_PATCHES);
    assert(pctl_phy_steps[4].arg0 == RK3576_PCTL_INIT_PATCH_ENTRY_COUNT);
    assert(pctl_phy_steps[4].arg1 == 33u);
    assert(pctl_phy_steps[6].phase == RK3576_DDR_PCTL_PHY_PROGRAM_FSP0);
    assert(pctl_phy_steps[6].offset == 0x03c8u);
    assert(pctl_phy_steps[9].channel == 1);
    assert(pctl_phy_steps[16].offset == 252u + 20u);
    assert(rk3576_ddr_plan_pctl_phy_timing(pctl_phy_steps, 4, active_map, RK3576_LPDDR4, 0, 2, 3, 1) == 7u);
    assert(pctl_phy_steps[0].offset == 0x200000u);
    assert(pctl_phy_steps[1].phase == RK3576_DDR_PCTL_PHY_WRITE_PCTL_LPDDR4);
    assert(pctl_phy_steps[2].arg0 == 2u);
    assert(pctl_phy_steps[3].phase == RK3576_DDR_PCTL_PHY_PROGRAM_RUNTIME_TAPS);
    assert(rk3576_ddr_plan_pctl_phy_timing(0, 0, active_map, RK3576_LPDDR4, 0, 2, 3, 1) == 7u);
    assert(rk3576_ddr_plan_pctl_phy_timing(pctl_phy_steps, 24, active_map, RK3576_DDR3, 1, 0, 0, 0) == 0);

    assert(rk3576_ddr_plan_rate_reprogram(rate_steps, 32, dual_active_map, RK3576_DDR3, 1) == 0);
    assert(rk3576_ddr_plan_rate_reprogram(rate_steps, 32, dual_active_map, RK3576_LPDDR5, 1) == 28u);
    assert(rate_steps[0].phase == RK3576_DDR_RATE_FORCE_GRF_WINDOW);
    assert(rate_steps[0].channel == RK3576_DISABLED_CHANNEL);
    assert(rate_steps[0].arg0 == 1u);
    assert(rate_steps[1].phase == RK3576_DDR_RATE_PROGRAM_PHY_PLL);
    assert(rate_steps[1].channel == 0);
    assert(rate_steps[1].arg0 == 24u);
    assert(rate_steps[4].phase == RK3576_DDR_RATE_CONFIGURE_PCTL);
    assert(rate_steps[4].arg0 == 1u);
    assert(rate_steps[8].phase == RK3576_DDR_RATE_PROGRAM_PHY_PLL);
    assert(rate_steps[8].channel == 1);
    assert(rate_steps[14].phase == RK3576_DDR_RATE_APPLY_LATE_MR_TIMING);
    assert(rate_steps[14].arg1 == 1u);
    assert(rate_steps[15].phase == RK3576_DDR_RATE_APPLY_COMMON_PERF);
    assert(rate_steps[20].phase == RK3576_DDR_RATE_WAIT_LPDDR5_SETTLE);
    assert(rate_steps[20].arg0 == 0x8a2u);
    assert(rate_steps[21].phase == RK3576_DDR_RATE_PROGRAM_LPDDR5_FREQ_BYTES);
    assert(rate_steps[23].phase == RK3576_DDR_RATE_PROGRAM_LPDDR5_FREQ_BYTES);
    assert(rate_steps[25].phase == RK3576_DDR_RATE_FORCE_GRF_WINDOW);
    assert(rate_steps[25].arg0 == 0);
    assert(rate_steps[27].phase == RK3576_DDR_RATE_FINAL_FREQ_PARAM);
    assert(rate_steps[27].channel == 1);
    assert(rk3576_ddr_plan_rate_reprogram(rate_steps, 8, active_map, RK3576_LPDDR4, 0) == 14u);
    assert(rate_steps[0].phase == RK3576_DDR_RATE_FORCE_GRF_WINDOW);
    assert(rate_steps[4].phase == RK3576_DDR_RATE_CONFIGURE_PCTL);
    assert(rate_steps[4].arg0 == 0);
    assert(rate_steps[7].phase == RK3576_DDR_RATE_APPLY_LATE_MR_TIMING);
    assert(rate_steps[7].arg1 == 0);
    assert(rate_steps[7].channel == 0);
    assert(rk3576_ddr_plan_rate_reprogram(0, 0, active_map, RK3576_LPDDR4, 0) == 14u);

    memset(&geometry, 0, sizeof(geometry));
    geometry.dram_type = RK3576_LPDDR5;
    geometry.channel[0].cs_count = 2;
    geometry.channel[0].col_bits = 10;
    geometry.channel[0].bank_bits_log2 = 3;
    geometry.channel[0].bus_width_code = 4;
    geometry.channel[0].die_bus_width_code = 1;
    geometry.channel[0].cs0_row_bits = 16;
    geometry.channel[0].cs1_row_bits = 17;
    geometry.channel[1].cs_count = 1;
    geometry.channel[1].col_bits = 11;
    geometry.channel[1].bank_bits_log2 = 4;
    geometry.channel[1].bus_width_code = 3;
    geometry.channel[1].die_bus_width_code = 0;
    geometry.channel[1].three_quarter_size_derate = 1;
    geometry.channel[1].cs0_row_bits = 15;
    addrmap_options.column_address_bits = 0x15;
    addrmap_options.rank_mode_code = 2;
    rk3576_ddr_build_geometry_registers(&geometry_regs, &geometry, dual_active_map,
                                        (uint32_t[2]){0x5du, 0x6eu}, &addrmap_options);
    assert(geometry_regs.pmu0_grf_word48 == ((RK3576_LPDDR5 << 27) | 0x1000u | 0x0100u | 1u));
    assert(geometry_regs.pmu0_grf_word4c_mr8 == 0x6e5du);
    assert(geometry_regs.pmu1_grf_addrmap_208 == 0xb582bac1u);
    assert(geometry_regs.pmu1_grf_addrmap_20c == 0x30002019u);
    assert(geometry_regs.pmu1_grf_addrmap_210 == 0);
    assert(geometry_regs.pmu1_grf_addrmap_214 == 0);
    assert(geometry_regs.cs_size_bytes[0][0] == (1ull << 33));
    assert(geometry_regs.cs_size_bytes[0][1] == (1ull << 34));
    assert(geometry_regs.cs_size_bytes[1][0] == (1ull << 33));
    assert(geometry_regs.cs_size_bytes[1][1] == 0);
    assert(geometry_regs.zq_channel_mask == 3u);
    assert(geometry_regs.final_control_channel_mask == 3u);
    rk3576_ddr_build_geometry_registers(&geometry_regs, &geometry, active_map,
                                        (uint32_t[2]){0x5du, 0x6eu}, &addrmap_options);
    assert(geometry_regs.pmu0_grf_word48 == (0xffff8000u | 0x1000u | 0x0100u | 1u));
    assert(geometry_regs.pmu0_grf_word4c_mr8 == 0x5du);
    assert(geometry_regs.zq_channel_mask == 1u);
    assert(geometry_regs.final_control_channel_mask == 1u);
    rk3576_ddr_build_ta52_mem_regions(&ta52, &geometry, ch1_active_map, 0);
    assert(ta52.region_count == 1u);
    assert(ta52.regions[0].base == ((3ull * (1ull << 33)) >> 2));

    assert(rk3576_ddr_capture_lpddr5_wck_mode(0, 0) == 0);
    assert(rk3576_ddr_capture_lpddr5_wck_mode(1, 0) == RK3576_DDR_WCK_MODE_DDRCTL_ACTIVE_FSP);
    assert(rk3576_ddr_capture_lpddr5_wck_mode(0, 0x8000u) == RK3576_DDR_WCK_MODE_PHY_HIGH_BIT);
    assert(rk3576_ddr_capture_lpddr5_wck_mode(0xffffffffu, 0x8000u) ==
           (RK3576_DDR_WCK_MODE_DDRCTL_ACTIVE_FSP | RK3576_DDR_WCK_MODE_PHY_HIGH_BIT));
    assert(rk3576_ddr_build_handoff_gate_steps(handoff_steps, 2, dual_active_map) == 2u);
    assert(handoff_steps[0].channel == 0);
    assert(handoff_steps[0].ddr_grf_gate_offset == 0x530u);
    assert(handoff_steps[0].ddr_grf_gate_value == 0x40004000u);
    assert(handoff_steps[0].pctl_offset == 0x1018cu);
    assert(handoff_steps[0].pctl_clear_mask == 0x3fu);
    assert(handoff_steps[0].ddr_grf_sideband_offset == 4u);
    assert(handoff_steps[0].ddr_grf_sideband_value == 0x00200020u);
    assert(handoff_steps[1].channel == 1);
    assert(handoff_steps[1].ddr_grf_gate_offset == 0x534u);
    assert(handoff_steps[1].ddr_grf_sideband_offset == 0x104u);
    assert(rk3576_ddr_build_handoff_gate_steps(0, 0, active_map) == 1u);

    for (uint32_t i = 0; i < RK3576_DDR_PHY_WORD_COUNT; ++i)
        phy_words[i] = 0x10000000u + i;
    memset(&dvfs_block, 0, sizeof(dvfs_block));
    dvfs_block.phy_fsp_mode = 0xabcdu;
    rk3576_ddr_snapshot_dvfs_channel(&dvfs_block, phy_words, 2);
    assert(dvfs_block.phy_fsp_mode == 0xabcdu);
    assert(dvfs_block.phy_training_status[0] == phy_words[748]);
    assert(dvfs_block.phy_training_status[4] == phy_words[752]);
    assert(dvfs_block.phy_dfi_select_word == phy_words[27]);
    assert(dvfs_block.per_rank_mode_pair[0][0] == phy_words[60]);
    assert(dvfs_block.per_rank_mode_pair[1][1] == phy_words[380]);
    assert(dvfs_block.rank_ca_window_a[0][8] == phy_words[121]);
    assert(dvfs_block.rank_ca_window_a[1][8] == phy_words[483]);
    assert(dvfs_block.rank_ca_window_b[0][0] == phy_words[388]);
    assert(dvfs_block.rank_gate_pair[0][0] == phy_words[349]);
    assert(dvfs_block.rank_gate_pair[1][1] == phy_words[474]);
    assert(dvfs_block.rank_dq_window_a[0][9] == phy_words[149]);
    assert(dvfs_block.rank_dq_window_a[1][9] == phy_words[285]);
    assert(dvfs_block.rank_dq_window_b[0][0] == phy_words[292]);
    assert(dvfs_block.rank_dq_window_b[1][9] == phy_words[345]);
    assert(dvfs_block.rank_dvfs_delay_pair[0][1] == phy_words[881]);
    assert(dvfs_block.rank_dvfs_delay_pair[1][0] == phy_words[882]);
    memset(&dvfs_block, 0xff, sizeof(dvfs_block));
    dvfs_block.phy_fsp_mode = 0x55u;
    rk3576_ddr_snapshot_dvfs_channel(&dvfs_block, phy_words, 1);
    assert(dvfs_block.phy_fsp_mode == 0x55u);
    assert(dvfs_block.per_rank_mode_pair[1][0] == 0);
    assert(dvfs_block.rank_dq_window_b[1][9] == 0);

    for (uint32_t i = 0; i < 27u; ++i)
        build_context_words[66 + i] = 0x30000000u + i;
    rk3576_ddr_copy_dvfs_channel_tail(&dvfs_tail, &build_context_words[66]);
    assert(dvfs_tail.derived_timing_words[0] == build_context_words[81]);
    assert(dvfs_tail.derived_timing_words[10] == build_context_words[91]);
    assert(dvfs_tail.reserved_2c == 0);

    memset(dvfs_records, 0, sizeof(dvfs_records));
    rk3576_ddr_seed_fsp0_phy_mode(dvfs_records, 1, 0x00e00000u);
    assert(dvfs_records[0].channel_blocks[1].phy_fsp_mode == 7u);
    assert(dvfs_records[3].channel_blocks[1].phy_fsp_mode == 7u);
    assert(dvfs_records[0].channel_blocks[0].phy_fsp_mode == 0);

    for (uint32_t i = 0; i < RK3576_DDR_CRU_WORD_COUNT; ++i)
        cru_words[i] = 0x20000000u + i;
    for (uint32_t i = 0; i < RK3576_DDR_BUILD_CONTEXT_WORD_COUNT; ++i)
        build_context_words[i] = 0x30000000u + i;
    cru_words[192] = 1u;
    phy_words[1] = 0x00401234u;
    build_context_words[66] = 1u;
    build_context_words[162] = 0x5au;
    build_context_words[163] = 0x6bu;
    build_context_words[164] = 0xa5u;
    build_context_words[165] = 0xb6u;
    rk3576_ddr_build_dvfs_fsp_record(&fsp_record, 1067, phy_words, cru_words, build_context_words, 2, dual_active_map);
    assert(fsp_record.magic == RK3576_DVFS_FSP_MAGIC);
    assert(fsp_record.actual_mhz == 1067u);
    assert(fsp_record.sentinel_minus1 == 0xffffffffu);
    assert(fsp_record.phy_reg_entries[0].off_mode1 == 0x0bf0u);
    assert(fsp_record.phy_reg_entries[0].value == phy_words[0x0bf4u / 4u]);
    assert(fsp_record.phy_reg_entries[4].value == (phy_words[0x0a58u / 4u] | 0x400u));
    assert(fsp_record.phy_reg_entries[7].value == phy_words[0x00c0u / 4u]);
    assert(fsp_record.phy_reg_entries[10].value == phy_words[0x0a04u / 4u]);
    assert(fsp_record.cru_pll_regs[0] == cru_words[8]);
    assert(fsp_record.cru_pll_regs[6] == cru_words[14]);
    assert(fsp_record.channel_mode_register_packed[0] == 0xa55au);
    assert(fsp_record.channel_mode_register_packed[1] == 0xb66bu);
    assert(fsp_record.phy_ctrl_bit22_snapshot == 0x00400000u);
    assert(fsp_record.timing_word_147 == build_context_words[147]);
    assert(fsp_record.timing_words_125_128[3] == build_context_words[128]);
    assert(fsp_record.mr_shadow_words[9] == build_context_words[138]);
    build_context_words[66] = 2u;
    cru_words[192] = 0;
    rk3576_ddr_build_dvfs_fsp_record(&fsp_record, 1067, phy_words, cru_words, build_context_words, 0, active_map);
    assert(fsp_record.phy_reg_entries[4].value == phy_words[0x0a38u / 4u]);
    assert(fsp_record.phy_reg_entries[7].value == 0);
    assert(fsp_record.cru_pll_regs[0] == cru_words[0]);
    assert(fsp_record.channel_mode_register_packed[1] == 0);

    assert(rk3576_ddr_build_early_clock_reset_steps(early_steps, 8) == 8u);
    assert(early_steps[0].op == RK3576_DDR_REG_SET_BITS);
    assert(early_steps[0].addr == RK3576_DDR_FW_SYSSGRF_BASE + 0x0004u);
    assert(early_steps[0].value == 0x00007000u);
    assert(early_steps[3].op == RK3576_DDR_REG_POLL_MASK_EQ);
    assert(early_steps[3].addr == RK3576_DDR_FW_SYSSGRF_BASE + 0x0ab0u);
    assert(early_steps[3].mask == 0x38u);
    assert(early_steps[3].value == 0x38u);
    assert(early_steps[5].addr == RK3576_DDR_CCI_GRF_BASE + 0x000cu);
    assert(early_steps[5].value == 0x07000700u);
    assert(early_steps[7].addr == RK3576_DDR_PMU1_GRF_BASE + 0x001cu);
    assert(early_steps[7].value == 24000000u);
    assert(rk3576_ddr_build_early_clock_reset_steps(0, 0) == 8u);
    assert(rk3576_ddr_eval_otp_gate(&otp_gate, 3, 0, 0, 0) == RK3576_DDR_STATUS_OK);
    assert(otp_gate.accepted == 1u);
    assert(otp_gate.writes_sys_sgrf_word300 == 1u);
    assert(otp_gate.sys_sgrf_word300 == 0x80078007u);
    assert(otp_gate.arms_secure_watchdog == 0);
    assert(rk3576_ddr_eval_otp_gate(&otp_gate, 3, 0x76354b52u, 0x101u, 0) == RK3576_DDR_STATUS_OK);
    assert(otp_gate.arms_secure_watchdog == 0);
    assert(rk3576_ddr_eval_otp_gate(&otp_gate, 3, 0x76354b52u, 0x10au, 0) == RK3576_DDR_STATUS_OK);
    assert(rk3576_ddr_eval_otp_gate(&otp_gate, 3, 0x76354b52u, 0x10du, 0) == RK3576_DDR_STATUS_OK);
    assert(rk3576_ddr_eval_otp_gate(&otp_gate, 3, 0x76354b52u, 0x113u, 0) == RK3576_DDR_STATUS_OK);
    assert(otp_gate.arms_secure_watchdog == 1u);
    assert(otp_gate.wdt_torr == 0x0000000cu);
    assert(otp_gate.wdt_cr == 0x00000009u);
    assert(otp_gate.wdt_crr == 0x00000076u);
    assert(rk3576_ddr_eval_otp_gate(&otp_gate, 3, 0x76354b52u, 0x113u, 10) == RK3576_DDR_STATUS_OK);
    assert(otp_gate.arms_secure_watchdog == 0);
    assert(rk3576_ddr_eval_otp_gate(&otp_gate, 2, 0x76354b52u, 0x101u, 0) == RK3576_DDR_STATUS_UNSUPPORTED);
    assert(rk3576_ddr_eval_otp_gate(&otp_gate, 3, 0x76354b52u, 0x102u, 0) == RK3576_DDR_STATUS_UNSUPPORTED);
    assert(rk3576_ddr_eval_otp_gate(&otp_gate, 3, 0x76313052u, 0x101u, 0) == RK3576_DDR_STATUS_UNSUPPORTED);

    debug_base = rk3576_ddr_select_debug_uart_base(0, RK3576_DDR_UART_CLOCK_HZ, debug_steps, 16, &debug_step_count, &debug_baud);
    assert(debug_base == RK3576_DDR_UART_BASE);
    assert(debug_baud == RK3576_DDR_UART_CLOCK_HZ);
    assert(debug_step_count == 4u);
    assert(debug_steps[0].addr == RK3576_DDR_CRU_BASE + 0x03f0u);
    assert(debug_steps[0].value == 0x07ff0300u);
    assert(debug_steps[1].addr == 0x26042010u);
    assert(debug_steps[1].value == 0x000f0009u);
    assert(debug_steps[2].value == 0x00f00090u);
    assert(debug_steps[3].addr == 0x26042030u);
    assert(debug_steps[3].value == 0x0c000c00u);
    debug_base = rk3576_ddr_select_debug_uart_base(0x1000u, RK3576_DDR_UART_CLOCK_HZ, debug_steps, 16, &debug_step_count, &debug_baud);
    assert(debug_base == RK3576_DDR_UART_BASE);
    assert(debug_step_count == 11u);
    assert(debug_steps[0].addr == RK3576_DDR_CRU_BASE + 0x0280u);
    assert(debug_steps[0].value == 0x000c0000u);
    assert(debug_steps[7].addr == RK3576_DDR_CRU_BASE + 0x03f0u);
    assert(debug_steps[7].value == 0x07ff0401u);
    debug_base = rk3576_ddr_select_debug_uart_base(0, 0x10000000u, debug_steps, 16, &debug_step_count, &debug_baud);
    assert(debug_base == 0);
    assert(debug_step_count == 1u);
    assert(debug_baud == 0);
    assert(rk3576_ddr_build_debug_uart_init_steps(debug_steps, 4, RK3576_DDR_UART_BASE, 115200u) == 4u);
    assert(debug_steps[0].addr == RK3576_DDR_UART_BASE + 0x0cu);
    assert(debug_steps[0].value == 0x83u);
    assert(debug_steps[1].addr == RK3576_DDR_UART_BASE);
    assert(debug_steps[1].value == 13u);
    assert(debug_steps[2].value == 3u);
    assert(debug_steps[3].addr == RK3576_DDR_UART_BASE + 0x98u);
    assert(debug_steps[3].value == 1u);
    assert(rk3576_ddr_build_debug_uart_init_steps(debug_steps, 4, RK3576_DDR_UART_BASE, 750000u) == 4u);
    assert(debug_steps[1].value == 2u);
    assert(rk3576_ddr_build_debug_uart_init_steps(debug_steps, 4, RK3576_DDR_UART_BASE, RK3576_DDR_UART_CLOCK_HZ) == 4u);
    assert(debug_steps[1].value == 1u);
    assert(rk3576_ddr_build_debug_uart_init_steps(debug_steps, 4, 0, RK3576_DDR_UART_CLOCK_HZ) == 0);

    assert(rk3576_ddr_eye_dq_count(0) == 8u);
    assert(rk3576_ddr_eye_dq_count(1) == 16u);
    assert(rk3576_ddr_eye_dq_count(2) == 32u);
    assert(rk3576_ddr_plan_eye_scan_override(debug_steps, 8, 1, 0, 0) == 8u);
    assert(debug_steps[0].op == RK3576_DDR_REG_CLEAR_BITS);
    assert(debug_steps[0].addr == RK3576_DDRCTL1_BASE + 0x10180u);
    assert(debug_steps[0].mask == 0xfffbfd00u);
    assert(debug_steps[1].op == RK3576_DDR_REG_POLL_MASK_EQ);
    assert(debug_steps[1].addr == RK3576_DDRCTL1_BASE + 0x10014u);
    assert(debug_steps[2].addr == RK3576_DDR_GRF_BASE + 0x118u);
    assert(debug_steps[2].value == 0x80000000u);
    assert(debug_steps[5].addr == RK3576_DDR_GRF_BASE + 0x534u);
    assert(debug_steps[6].addr == RK3576_DDR_HWLP1_BASE);
    assert(debug_steps[7].addr == RK3576_DDR_HWLP1_BASE + 0x0cu);
    eye_saved.ddrctl_10180 = 0x12345678u;
    eye_saved.ddr_grf_018_low_ff7f = 0x00001234u;
    eye_saved.ddr_grf_000_low_1f00 = 0x00001000u;
    eye_saved.ddr_grf_004_low_90c6 = 0x00008002u;
    eye_saved.ddr_grf_lane_low_4000 = 0x00004000u;
    eye_saved.hwlp_word0 = 0xa5a50000u;
    eye_saved.hwlp_word3 = 0x5a5a0003u;
    assert(rk3576_ddr_plan_eye_scan_override(debug_steps, 8, 0, 1, &eye_saved) == 7u);
    assert(debug_steps[0].addr == RK3576_DDR_GRF_BASE + 0x18u);
    assert(debug_steps[0].value == 0xff7f1234u);
    assert(debug_steps[1].value == 0x1f001000u);
    assert(debug_steps[2].value == 0x90c68002u);
    assert(debug_steps[3].addr == RK3576_DDR_GRF_BASE + 0x530u);
    assert(debug_steps[3].value == 0x40004000u);
    assert(debug_steps[4].addr == RK3576_DDRCTL0_BASE + 0x10180u);
    assert(debug_steps[4].value == 0x12345678u);

    memset(dq_roc_candidates, 0, sizeof(dq_roc_candidates));
    dq_roc_candidates[0].p_code = 1;
    dq_roc_candidates[0].n_code = 2;
    dq_roc_candidates[0].width[0] = 12;
    dq_roc_candidates[0].width[8] = 25;
    dq_roc_candidates[0].width[17] = 9;
    dq_roc_candidates[1].p_code = 3;
    dq_roc_candidates[1].n_code = 4;
    dq_roc_candidates[1].width[0] = 11;
    dq_roc_candidates[1].width[8] = 26;
    dq_roc_candidates[1].width[17] = 15;
    dq_roc_candidates[2].p_code = 5;
    dq_roc_candidates[2].n_code = 6;
    dq_roc_candidates[2].width[0] = 12;
    dq_roc_candidates[2].width[8] = 1;
    dq_roc_candidates[2].width[17] = 16;
    rk3576_ddr_select_dq_roc(roc_best, dq_roc_candidates, 3);
    assert(roc_best[0].p_code == 1);
    assert(roc_best[0].n_code == 2);
    assert(roc_best[0].width == 12);
    assert(roc_best[8].p_code == 3);
    assert(roc_best[8].n_code == 4);
    assert(roc_best[8].width == 26);
    assert(roc_best[17].p_code == 5);
    assert(roc_best[17].n_code == 6);
    assert(roc_best[17].width == 16);
    assert(rk3576_ddr_pack_dq_roc_word(0xffffffffu, &roc_best[8]) == ((0xffffffffu & 0xfffffe07u) | (3u << 3) | (4u << 6)));

    dqs_roc_candidates[0].p_code = 1;
    dqs_roc_candidates[0].n_code = 1;
    dqs_roc_candidates[0].low_lane_min_width = 20;
    dqs_roc_candidates[0].high_lane_min_width = 30;
    dqs_roc_candidates[1].p_code = 2;
    dqs_roc_candidates[1].n_code = 3;
    dqs_roc_candidates[1].low_lane_min_width = 25;
    dqs_roc_candidates[1].high_lane_min_width = 29;
    dqs_roc_candidates[2].p_code = 3;
    dqs_roc_candidates[2].n_code = 0;
    dqs_roc_candidates[2].low_lane_min_width = 24;
    dqs_roc_candidates[2].high_lane_min_width = 35;
    rk3576_ddr_select_dqs_roc(roc_best, dqs_roc_candidates, 3);
    assert(roc_best[0].p_code == 2);
    assert(roc_best[0].n_code == 3);
    assert(roc_best[0].width == 25);
    assert(roc_best[1].p_code == 3);
    assert(roc_best[1].n_code == 0);
    assert(roc_best[1].width == 35);
    assert(rk3576_ddr_pack_dqs_roc_word(0xffffffffu, &roc_best[1]) == ((0xffffffffu & 0xfffffe4fu) | (3u << 4)));

    eye_rows[0] = (rk3576_ddr_eye_window_row){500, -12, 18};
    eye_rows[1] = (rk3576_ddr_eye_window_row){480, -20, 40};
    eye_rows[2] = (rk3576_ddr_eye_window_row){460, -5, 10};
    eye_rows[3] = (rk3576_ddr_eye_window_row){0, 0, 0};
    assert(rk3576_ddr_summarize_eye_rows(&eye_summary, eye_rows, 4, 470, 450) == RK3576_DDR_STATUS_OK);
    assert(eye_summary.valid_rows == 3u);
    assert(eye_summary.best_row == 1u);
    assert(eye_summary.best_vref_x10 == 480);
    assert(eye_summary.best_width == 60);
    assert(eye_summary.current_row == 1u);
    assert(eye_summary.current_vref_x10 == 480);
    assert(eye_summary.first_vref_x10 == 500);
    assert(eye_summary.fail_mask == 0);
    assert(rk3576_ddr_summarize_eye_rows(&eye_summary, eye_rows, 4, 455, 520) == RK3576_DDR_STATUS_UNSUPPORTED);
    assert(eye_summary.fail_mask == RK3576_DDR_EYE_FAIL_MIN_VREF);
    memset(&eye_limits, 0, sizeof(eye_limits));
    eye_limits.rd_left_limit = 16;
    eye_limits.rd_right_limit = 16;
    eye_limits.wr_right_limit = 5;
    eye_limits.min_eye_threshold = 450;
    assert(rk3576_ddr_summarize_eye_report(&eye_summary, eye_rows, 4, 480, &eye_limits, 0) == RK3576_DDR_STATUS_OK);
    eye_rows_narrow[0] = eye_rows[0];
    eye_rows_narrow[1] = (rk3576_ddr_eye_window_row){480, -10, 12};
    eye_rows_narrow[2] = eye_rows[2];
    eye_rows_narrow[3] = eye_rows[3];
    assert(rk3576_ddr_summarize_eye_report(&eye_summary, eye_rows_narrow, 4, 480, &eye_limits, 0) == RK3576_DDR_STATUS_UNSUPPORTED);
    assert((eye_summary.fail_mask & RK3576_DDR_EYE_FAIL_MARGIN) != 0);
    assert(rk3576_ddr_intersect_eye_rows(eye_all_rows, eye_rows, 4, 0) == 3u);
    assert(eye_all_rows[1].vref_x10 == 480);
    assert(eye_all_rows[1].delay_min == -20);
    assert(eye_all_rows[1].delay_max == 40);
    assert(rk3576_ddr_intersect_eye_rows(eye_all_rows, eye_rows_narrow, 4, 1) == 3u);
    assert(eye_all_rows[1].delay_min == -10);
    assert(eye_all_rows[1].delay_max == 12);
    eye_rows_narrow[1].vref_x10 = 470;
    assert(rk3576_ddr_intersect_eye_rows(eye_all_rows, eye_rows_narrow, 4, 1) == 2u);
    assert(eye_all_rows[1].delay_min == 1024);
    assert(eye_all_rows[1].delay_max == -1024);
    assert(rk3576_ddr_summarize_eye_metric(&eye_metric, eye_metric_values, 4) == RK3576_DDR_STATUS_OK);
    assert(eye_metric.count == 4u);
    assert(eye_metric.min_index == 1u);
    assert(eye_metric.min_value == 430);
    assert(eye_metric.max_value == 480);
    assert(eye_metric.avg_value_x10 == 450);

    assert(rk3576_ddr_sram_boot_state_is_normal(0));
    assert(rk3576_ddr_sram_boot_state_is_normal(0x80));
    assert(!rk3576_ddr_sram_boot_state_is_normal(10));
    assert(!rk3576_ddr_sram_boot_state_is_normal(0x81));
    assert(rk3576_ddr_warm_reentry_rejects_init(0xef08a53cu, 0));
    assert(!rk3576_ddr_warm_reentry_rejects_init(0xef08a53cu, 10));
    assert(rk3576_ddr_select_timer_hz(0) == 24000000ull);
    assert(rk3576_ddr_select_timer_hz(0x100u) == 1000000000ull);

    assert(rk3576_ddr_uart_crlf_transform(crlf_out, sizeof(crlf_out), "a\nb") == 4u);
    assert(strcmp(crlf_out, "a\r\nb") == 0);
    assert(rk3576_ddr_debug_format(fmt_out, sizeof(fmt_out), "%s %x %d", (rk3576_ddr_debug_arg[]){{0, "v"}, {0x2a, 0}, {0xfffffffeu, 0}}, 3) == 7u);
    assert(strcmp(fmt_out, "v 2a -2") == 0);
    assert(rk3576_ddr_debug_format(fmt_out, sizeof(fmt_out), "%u %o", (rk3576_ddr_debug_arg[]){{0xfffffffeu, 0}, {010u, 0}}, 2) == 4u);
    assert(strcmp(fmt_out, "-2 8") == 0);

    assert(rk3576_ddr_copy_export_image(export_with_header, export_payload, RK3576_DDR_EXPORT_PAYLOAD_LEN,
                                       RK3576_LPDDR5) == export_with_header + sizeof(rk3576_ddr_export_header));
    assert(load32(export_with_header) == RK3576_DDR_EXPORT_PAYLOAD_LEN);
    assert(load32(export_with_header + 4) == RK3576_LPDDR5);
    assert(memcmp(export_with_header + sizeof(rk3576_ddr_export_header), export_payload,
                  RK3576_DDR_EXPORT_PAYLOAD_LEN) == 0);

    return 0;
}
