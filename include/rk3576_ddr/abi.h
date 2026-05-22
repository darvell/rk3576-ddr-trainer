#pragma once

#include <stddef.h>
#include <stdint.h>

#define RK3576_DDR_CONFIG_TAG_AREA_SIZE 0x2000u
#define RK3576_DDR_EXPORT_PAYLOAD_LEN   0x00000B8Cu
#define RK3576_DDR_EXPORT_TOTAL_LEN     0x00000B9Cu

#define RK3576_DDR_MAX_CHANNELS 2u
#define RK3576_DDR_MAX_RANKS    4u


#define RK3576_TAG_MAGIC_CONTAINER 0x54410001u
#define RK3576_TAG_FIRST_WRITABLE  0x54410050u
#define RK3576_TAG_LAST_WRITABLE   0x5441005Au
#define RK3576_TAG_HASH_SEED       0x47C6A9E6u

#define RK3576_TAG_BOOT_DEFAULTS 0x54410050u
#define RK3576_TAG_MEM_REGION    0x54410052u
#define RK3576_TAG_BOOT_EXPORT   0x54410058u
#define RK3576_TAG_STATIC_INFO   0x5441005Au

#define RK3576_DISABLED_CHANNEL 15u

typedef enum rk3576_ddr_status {
    RK3576_DDR_STATUS_OK = 0,
    RK3576_DDR_STATUS_UNSUPPORTED = -1,
    RK3576_DDR_STATUS_NO_SPACE = -12,
    RK3576_DDR_STATUS_INVALID_ARGUMENT = -22,
    RK3576_DDR_STATUS_MISSING_PAYLOAD = -61,
} rk3576_ddr_status;

typedef enum rk3576_ddr_dram_kind {
    RK3576_DDR4 = 0u,
    RK3576_DDR2 = 2u,
    RK3576_DDR3 = 3u,
    RK3576_LPDDR2 = 5u,
    RK3576_LPDDR3 = 6u,
    RK3576_LPDDR4 = 7u,
    RK3576_LPDDR4X = 8u,
    RK3576_LPDDR5 = 9u,
} rk3576_ddr_dram_kind;

#define RK3576_LPDDR4_PHY_FREQ_ROW_COUNT 4u
#define RK3576_LPDDR5_PHY_FREQ_ROW_COUNT 5u
#define RK3576_LPDDR5_WCK_LOW_ENTRIES    6u
#define RK3576_LPDDR5_WCK_HIGH_ENTRIES   10u
#define RK3576_PCTL_INIT_PATCH_ENTRY_COUNT 54u
#define RK3576_PCTL_INIT_PATCH_VALID_MAX_OFFSET 0x003fffffu
#define RK3576_DVFS_FSP_REG_ENTRY_COUNT 11u
#define RK3576_DVFS_FSP_RECORD_COUNT    4u
#define RK3576_DVFS_FSP_MAGIC           0xFEAD0004u

typedef struct rk3576_ddr_boot_params {
    uint32_t word0;
    uint32_t word4;
    uint32_t timer_flags_word8;
    uint32_t wordc;
    uint8_t preserved_10_1f[0x10];
} rk3576_ddr_boot_params;

typedef struct rk3576_ddr_tag_record_header {
    uint32_t word_count;
    uint32_t tag;
} rk3576_ddr_tag_record_header;

typedef struct rk3576_ddr_export_header {
    uint32_t payload_len;
    uint32_t dram_type;
    uint32_t reserved_08;
    uint32_t reserved_0c;
} rk3576_ddr_export_header;

typedef struct rk3576_ddr_reg_patch {
    uint32_t register_offset;
    uint32_t clear_mask;
    uint32_t set_value;
} rk3576_ddr_reg_patch;

typedef struct __attribute__((packed)) rk3576_ddr_phy_freq_row {
    uint8_t byte[21];
} rk3576_ddr_phy_freq_row;

typedef struct rk3576_ddr_lp5_wck_entry {
    uint32_t max_target_mhz_x2;
    uint32_t byte_08_value;
    uint32_t byte_00_value;
    uint32_t nibble_1c_value;
    uint32_t byte_10_value;
    uint32_t byte_18_value;
} rk3576_ddr_lp5_wck_entry;

typedef struct rk3576_ddr_dvfs_fsp_reg_offsets {
    uint32_t off_mode1;
    uint32_t off_mode2;
    uint32_t off_mode0;
} rk3576_ddr_dvfs_fsp_reg_offsets;

typedef struct __attribute__((packed)) rk3576_ddr_ta50_boot_defaults {
    uint32_t reserved_00;
    uint32_t enabled_flag;
    uint64_t selected_base;
    uint32_t timing_word_low24;
    uint32_t timing_word_high4;
    uint32_t reserved_18;
    uint32_t reserved_1c;
    uint32_t reserved_20;
} rk3576_ddr_ta50_boot_defaults;

typedef struct rk3576_ddr_mem_region {
    uint64_t base;
    uint64_t size;
} rk3576_ddr_mem_region;

typedef struct __attribute__((packed)) rk3576_ddr_ta52_mem_regions {
    uint32_t region_count;
    uint32_t reserved_04;
    uint64_t dram_base;
    rk3576_ddr_mem_region regions[4];
    uint8_t reserved_50_a7[0x58];
    uint32_t flags;
    uint32_t reserved_ac_b3[2];
} rk3576_ddr_ta52_mem_regions;

typedef struct rk3576_ddr_ta58_boot_export {
    uint32_t reserved_00;
    uint32_t boot_word0;
    uint32_t boot_word4;
    uint32_t boot_word8_timer_flags;
    uint32_t boot_wordc;
    uint32_t reserved_14_33[8];
} rk3576_ddr_ta58_boot_export;

typedef struct rk3576_ddr_ta5a_static_info {
    uint32_t reserved_00;
    char build_id[0x15];
    uint8_t reserved_19_123[0x10b];
} rk3576_ddr_ta5a_static_info;

typedef struct rk3576_ddr_channel_geometry {
    uint32_t cs_count;
    uint32_t col_bits;
    uint32_t bank_bits_log2;
    uint32_t bus_width_code;
    uint32_t die_bus_width_code;
    uint32_t three_quarter_size_derate;
    uint32_t cs0_row_bits;
    uint32_t cs1_row_bits;
    uint32_t cs2_row_bits;
    uint32_t cs3_row_bits;
    uint32_t cs0_alt_row_bits;
    uint32_t cs1_alt_row_bits;
    uint32_t cs2_alt_row_bits;
    uint32_t cs3_alt_row_bits;
    uint32_t training_mode_code;
    uint32_t reserved_3c_68[12];
} rk3576_ddr_channel_geometry;

typedef struct rk3576_ddr_runtime_geometry {
    rk3576_ddr_channel_geometry channel[2];
    uint32_t geometry_flags;
    uint32_t dram_type;
    uint32_t reserved_108;
    uint32_t reserved_10c;
    uint64_t platform_data;
} rk3576_ddr_runtime_geometry;

_Static_assert(sizeof(rk3576_ddr_boot_params) == 0x20, "boot params layout");
_Static_assert(sizeof(rk3576_ddr_tag_record_header) == 0x08, "tag header layout");
_Static_assert(sizeof(rk3576_ddr_export_header) == 0x10, "export header layout");
_Static_assert(sizeof(rk3576_ddr_reg_patch) == 0x0c, "patch entry layout");
_Static_assert(sizeof(rk3576_ddr_phy_freq_row) == 0x15, "phy row layout");
_Static_assert(sizeof(rk3576_ddr_lp5_wck_entry) == 0x18, "wck entry layout");
_Static_assert(sizeof(rk3576_ddr_dvfs_fsp_reg_offsets) == 0x0c, "DVFS register offset row layout");
_Static_assert(sizeof(rk3576_ddr_ta50_boot_defaults) == 0x24, "TA50 layout");
_Static_assert(sizeof(rk3576_ddr_ta52_mem_regions) == 0xb4, "TA52 layout");
_Static_assert(sizeof(rk3576_ddr_ta58_boot_export) == 0x34, "TA58 layout");
_Static_assert(sizeof(rk3576_ddr_ta5a_static_info) == 0x124, "TA5A layout");
_Static_assert(sizeof(rk3576_ddr_channel_geometry) == 0x6c, "channel geometry layout");
_Static_assert(sizeof(rk3576_ddr_runtime_geometry) == 0xf0, "runtime geometry layout");
