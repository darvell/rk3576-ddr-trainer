#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rk3576_ddr/abi.h"

uint32_t rk3576_ddr_tag_hash32(const void *buf, uint32_t len);
int rk3576_ddr_tag_is_valid(uint32_t tag);
uint8_t rk3576_ddr_tag_record_word_count(uint32_t tag);
size_t rk3576_ddr_tag_payload_size(uint32_t tag);
void rk3576_ddr_tag_area_clear(uint8_t area[RK3576_DDR_CONFIG_TAG_AREA_SIZE]);
int rk3576_ddr_tag_area_write(uint8_t area[RK3576_DDR_CONFIG_TAG_AREA_SIZE], uint32_t tag, const void *payload);
void *rk3576_ddr_copy_export_image(void *dst, const void *payload, uint32_t payload_len, uint32_t dram_type);

void rk3576_ddr_build_ta50_boot_defaults(rk3576_ddr_ta50_boot_defaults *payload, uint64_t selected_base, uint32_t timing_word);
void rk3576_ddr_build_ta52_mem_regions(rk3576_ddr_ta52_mem_regions *payload, const rk3576_ddr_runtime_geometry *geometry, const uint32_t active_channel_map[2], uint32_t sys_sgrf_word0);
void rk3576_ddr_build_ta58_boot_export(rk3576_ddr_ta58_boot_export *payload, const rk3576_ddr_boot_params *boot);
void rk3576_ddr_build_ta5a_static_info(rk3576_ddr_ta5a_static_info *payload, const char *build_id);
int rk3576_ddr_emit_config_tags(uint8_t area[RK3576_DDR_CONFIG_TAG_AREA_SIZE], const rk3576_ddr_runtime_geometry *geometry, const uint32_t active_channel_map[2], uint32_t sys_sgrf_word0, const rk3576_ddr_boot_params *boot);

uint64_t rk3576_ddr_channel_cs_size(const rk3576_ddr_channel_geometry *channel, uint32_t cs_index, uint32_t dram_type);
