#include "rk3576_ddr/tags.h"

#include "rk3576_ddr/tables.h"
#include "rk3576_ddr/types.h"

#include <string.h>

enum {
    RK3576_DDR_TAG_CONTAINER_WORDS = 5u,
    RK3576_DDR_TAG_HEADER_BYTES = 8u,
    RK3576_DDR_TAG_HASH_BYTES = 4u,
    RK3576_DDR_TAG_CONTAINER_NEXT_OFFSET = 12u,
    RK3576_DDR_TAG_CONTAINER_RESERVED_OFFSET = 16u,
    RK3576_DDR_TAG_FIRST_RECORD_OFFSET = 20u,
    RK3576_DDR_TAG_CLEAR_BYTES = 0x32cu,
    RK3576_DDR_TAG_STATIC_INFO_FLAG = 2u,
    RK3576_DDR_TIMING_WORD_LOW_MASK = 0x00ffffffu,
    RK3576_DDR_TIMING_WORD_HIGH_SHIFT = 24u,
    RK3576_DDR_TIMING_WORD_HIGH_MASK = 0x0fu,
    RK3576_DDR_SGRF_REMAP_SHIFT = 4u,
    RK3576_DDR_SGRF_REMAP_MASK = 0x0fu,
    RK3576_DDR_TAG_MAGIC_MAX = 0x544100ffu,
    RK3576_DDR_CS_INDEX_0 = 0u,
    RK3576_DDR_CS_INDEX_1 = 1u,
    RK3576_DDR_CS_INDEX_2 = 2u,
    RK3576_DDR_CS_INDEX_3 = 3u,
    RK3576_DDR_SINGLE_CS_COUNT = 1u,
    RK3576_DDR_QUAD_CS_COUNT = 4u,
};

#define RK3576_DDR_DEFAULT_BASE 0x40000000ull
#define RK3576_DDR_HIGH_ALIAS_BASE 0x440000000ull
#define RK3576_DDR_DEFAULT_DEBUG_BASE 0x2ad40000ull
#define RK3576_DDR_DEFAULT_DEBUG_CLOCK_HZ 1500000u

static uint32_t load32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static void store32(uint8_t *p, uint32_t v)
{
    memcpy(p, &v, sizeof(v));
}

static size_t bounded_strlen(const char *s, size_t max_len)
{
    size_t len = 0;

    while (len < max_len && s[len])
        ++len;
    return len;
}

uint32_t rk3576_ddr_tag_hash32(const void *buf, uint32_t len)
{
    uint32_t h = RK3576_TAG_HASH_SEED;
    const uint8_t *bytes = (const uint8_t *)buf;

    if (!bytes || !len)
        return h;

    for (uint32_t i = 0; i < len; ++i)
        h ^= (h >> 2) + 32u * h + bytes[i];

    return h;
}

int rk3576_ddr_tag_is_valid(uint32_t tag)
{
    return tag == 0 || tag == RK3576_TAG_MAGIC_CONTAINER ||
           (tag >= RK3576_TAG_FIRST_WRITABLE && tag <= RK3576_DDR_TAG_MAGIC_MAX);
}

uint8_t rk3576_ddr_tag_record_word_count(uint32_t tag)
{
    if (tag < RK3576_TAG_FIRST_WRITABLE || tag > RK3576_TAG_LAST_WRITABLE)
        return 0;
    return rk3576_ddr_tag_record_words[tag - RK3576_TAG_FIRST_WRITABLE];
}

size_t rk3576_ddr_tag_payload_size(uint32_t tag)
{
    uint8_t words = rk3576_ddr_tag_record_word_count(tag);
    return words ? (size_t)words * sizeof(uint32_t) - RK3576_DDR_TAG_HEADER_BYTES - RK3576_DDR_TAG_HASH_BYTES : 0;
}

void rk3576_ddr_tag_area_clear(uint8_t area[RK3576_DDR_CONFIG_TAG_AREA_SIZE])
{
    memset(area, 0, RK3576_DDR_TAG_CLEAR_BYTES);
}

int rk3576_ddr_tag_area_write(uint8_t area[RK3576_DDR_CONFIG_TAG_AREA_SIZE], uint32_t tag, const void *payload)
{
    size_t off;
    int append_terminator;
    uint8_t words;
    size_t record_bytes;

    if (!payload)
        return RK3576_DDR_STATUS_MISSING_PAYLOAD;
    if (!rk3576_ddr_tag_is_valid(tag))
        return RK3576_DDR_STATUS_INVALID_ARGUMENT;
    if (tag == 0 || tag == RK3576_TAG_MAGIC_CONTAINER)
        return RK3576_DDR_STATUS_UNSUPPORTED;

    if (load32(area + 4) == RK3576_TAG_MAGIC_CONTAINER) {
        off = 0;
        append_terminator = 0;
        while (load32(area + off) != 0) {
            uint32_t current_words = load32(area + off);
            uint32_t current_tag;

            if (off + RK3576_DDR_TAG_HEADER_BYTES > RK3576_DDR_CONFIG_TAG_AREA_SIZE || current_words == 0 ||
                off + (size_t)current_words * sizeof(uint32_t) > RK3576_DDR_CONFIG_TAG_AREA_SIZE)
                return RK3576_DDR_STATUS_INVALID_ARGUMENT;

            current_tag = load32(area + off + sizeof(uint32_t));
            if (!rk3576_ddr_tag_is_valid(current_tag))
                return RK3576_DDR_STATUS_INVALID_ARGUMENT;
            if (current_tag == tag)
                break;
            if (current_tag == 0)
                break;
            off += (size_t)current_words * sizeof(uint32_t);
            append_terminator = 1;
        }
    } else {
        store32(area, RK3576_DDR_TAG_CONTAINER_WORDS);
        store32(area + sizeof(uint32_t), RK3576_TAG_MAGIC_CONTAINER);
        store32(area + RK3576_DDR_TAG_CONTAINER_NEXT_OFFSET, 0u);
        store32(area + RK3576_DDR_TAG_CONTAINER_RESERVED_OFFSET, 0u);
        off = RK3576_DDR_TAG_FIRST_RECORD_OFFSET;
        append_terminator = 1;
    }

    words = rk3576_ddr_tag_record_word_count(tag);
    if (!words)
        return RK3576_DDR_STATUS_INVALID_ARGUMENT;

    record_bytes = (size_t)words * sizeof(uint32_t);
    if (off + record_bytes > RK3576_DDR_CONFIG_TAG_AREA_SIZE)
        return RK3576_DDR_STATUS_NO_SPACE;

    store32(area + off, words);
    store32(area + off + sizeof(uint32_t), tag);
    memcpy(area + off + RK3576_DDR_TAG_HEADER_BYTES, payload,
           record_bytes - RK3576_DDR_TAG_HEADER_BYTES - RK3576_DDR_TAG_HASH_BYTES);
    store32(area + off + record_bytes - RK3576_DDR_TAG_HASH_BYTES,
            rk3576_ddr_tag_hash32(area + off, (uint32_t)record_bytes - RK3576_DDR_TAG_HASH_BYTES));

    if (append_terminator) {
        size_t end = off + record_bytes;
        if (end + RK3576_DDR_TAG_HEADER_BYTES <= RK3576_DDR_CONFIG_TAG_AREA_SIZE) {
            store32(area + end, 0u);
            store32(area + end + sizeof(uint32_t), 0u);
        }
    }

    return 0;
}

void *rk3576_ddr_copy_export_image(void *dst, const void *payload, uint32_t payload_len, uint32_t dram_type)
{
    uint32_t header[4] = {payload_len, dram_type, 0u, 0u};

    memcpy(dst, header, sizeof(header));
    memcpy((uint8_t *)dst + sizeof(header), payload, payload_len);
    return (uint8_t *)dst + sizeof(header);
}

void rk3576_ddr_build_ta50_boot_defaults(rk3576_ddr_ta50_boot_defaults *payload, uint64_t selected_base, uint32_t timing_word)
{
    memset(payload, 0, sizeof(*payload));
    payload->enabled_flag = 1u;
    payload->selected_base = selected_base;
    payload->timing_word_low24 = timing_word & RK3576_DDR_TIMING_WORD_LOW_MASK;
    payload->timing_word_high4 = (timing_word >> RK3576_DDR_TIMING_WORD_HIGH_SHIFT) & RK3576_DDR_TIMING_WORD_HIGH_MASK;
}

void rk3576_ddr_build_ta58_boot_export(rk3576_ddr_ta58_boot_export *payload, const rk3576_ddr_boot_params *boot)
{
    memset(payload, 0, sizeof(*payload));
    payload->boot_word0 = boot->word0;
    payload->boot_word4 = boot->word4;
    payload->boot_word8_timer_flags = boot->timer_flags_word8;
    payload->boot_wordc = boot->wordc;
}

void rk3576_ddr_build_ta5a_static_info(rk3576_ddr_ta5a_static_info *payload, const char *build_id)
{
    memset(payload, 0, sizeof(*payload));
    if (build_id)
        memcpy(payload->build_id, build_id, bounded_strlen(build_id, sizeof(payload->build_id)));
}

uint64_t rk3576_ddr_channel_cs_size(const rk3576_ddr_channel_geometry *channel, uint32_t cs_index, uint32_t dram_type)
{
    uint32_t ddr4_die_width_extra = 0;
    uint32_t base_shift;
    uint64_t cs0;
    uint64_t cs1 = 0;
    uint64_t cs2 = 0;
    uint64_t cs3 = 0;

    if (dram_type == RK3576_DDR4)
        ddr4_die_width_extra = (channel->die_bus_width_code == 0) + 1u;

    base_shift = channel->bus_width_code + channel->col_bits + channel->bank_bits_log2 + ddr4_die_width_extra;
    cs0 = 1ull << (uint8_t)(base_shift + channel->cs0_row_bits);

    if (channel->cs_count > RK3576_DDR_SINGLE_CS_COUNT) {
        cs1 = 1ull << (uint8_t)(base_shift + channel->cs1_row_bits);
        if (channel->cs_count == RK3576_DDR_QUAD_CS_COUNT) {
            cs2 = 1ull << (uint8_t)(base_shift + channel->cs2_row_bits);
            cs3 = 1ull << (uint8_t)(base_shift + channel->cs3_row_bits);
        }
    }

    switch (cs_index) {
    case RK3576_DDR_CS_INDEX_0:
        return cs0;
    case RK3576_DDR_CS_INDEX_1:
        return cs1;
    case RK3576_DDR_CS_INDEX_2:
        return cs2;
    case RK3576_DDR_CS_INDEX_3:
        return cs3;
    default:
        return cs0 + cs1 + cs2 + cs3;
    }
}

void rk3576_ddr_build_ta52_mem_regions(rk3576_ddr_ta52_mem_regions *payload, const rk3576_ddr_runtime_geometry *geometry, const uint32_t active_channel_map[2], uint32_t sys_sgrf_word0)
{
    uint64_t total[2] = {0, 0};
    uint64_t larger_cs1[2] = {0, 0};
    uint64_t cs0[2] = {0, 0};
    uint64_t cs1[2] = {0, 0};
    uint32_t active_count = 0;
    uint32_t remap_nibble = (sys_sgrf_word0 >> RK3576_DDR_SGRF_REMAP_SHIFT) & RK3576_DDR_SGRF_REMAP_MASK;
    uint32_t dram_type = geometry->dram_type;

    for (uint32_t slot = 0; slot < 2u; ++slot) {
        uint32_t ch = active_channel_map[slot];
        uint32_t out = active_count;

        if (!rk3576_ddr_channel_is_active(ch))
            continue;

        cs0[out] = rk3576_ddr_channel_cs_size(&geometry->channel[ch], 0, dram_type);
        cs1[out] = rk3576_ddr_channel_cs_size(&geometry->channel[ch], 1, dram_type);
        ++active_count;

        if (cs0[out] < cs1[out])
            larger_cs1[out] = cs1[out];

        if (geometry->channel[ch].three_quarter_size_derate) {
            cs0[out] = (3u * cs0[out]) >> 2;
            cs1[out] = (3u * cs1[out]) >> 2;
        }

        total[out] = cs0[out] + cs1[out];
    }

    memset(payload, 0, sizeof(*payload));
    payload->dram_base = RK3576_DDR_DEFAULT_BASE;

    if (!remap_nibble) {
        if (active_count == 2u) {
            if (larger_cs1[0]) {
                payload->region_count = 4u;
                payload->regions[0].base = larger_cs1[0] + RK3576_DDR_DEFAULT_BASE;
                payload->regions[0].size = RK3576_DDR_HIGH_ALIAS_BASE;
                payload->regions[1].base = larger_cs1[1] + RK3576_DDR_HIGH_ALIAS_BASE;
                payload->regions[1].size = cs0[0];
                payload->regions[2].base = cs1[0];
                payload->regions[2].size = cs0[1];
                payload->regions[3].base = cs1[1];
            } else {
                payload->region_count = 2u;
                payload->regions[0].base = RK3576_DDR_HIGH_ALIAS_BASE;
                payload->regions[0].size = total[0];
                payload->regions[1].base = total[1];
            }
        } else if (larger_cs1[0]) {
            payload->region_count = 2u;
            payload->regions[0].base = larger_cs1[0] + RK3576_DDR_DEFAULT_BASE;
            payload->regions[0].size = cs0[0];
            payload->regions[1].base = cs1[0];
        } else {
            payload->region_count = 1u;
            payload->regions[0].base = total[0];
        }
    } else if (!larger_cs1[0]) {
        payload->region_count = 1u;
        payload->regions[0].base = total[0] + total[1];
    } else {
        payload->region_count = 2u;
        payload->regions[0].base = larger_cs1[0] + larger_cs1[1] + RK3576_DDR_DEFAULT_BASE;
        payload->regions[0].size = cs0[0] + cs0[1];
        payload->regions[1].base = cs1[0] + cs1[1];
    }

    payload->flags |= RK3576_DDR_TAG_STATIC_INFO_FLAG;
}

int rk3576_ddr_emit_config_tags(uint8_t area[RK3576_DDR_CONFIG_TAG_AREA_SIZE], const rk3576_ddr_runtime_geometry *geometry, const uint32_t active_channel_map[2], uint32_t sys_sgrf_word0, const rk3576_ddr_boot_params *boot)
{
    rk3576_ddr_ta50_boot_defaults ta50;
    rk3576_ddr_ta52_mem_regions ta52;
    rk3576_ddr_ta58_boot_export ta58;
    rk3576_ddr_ta5a_static_info ta5a;
    int rc;

    rk3576_ddr_tag_area_clear(area);

    rk3576_ddr_build_ta50_boot_defaults(&ta50, RK3576_DDR_DEFAULT_DEBUG_BASE, RK3576_DDR_DEFAULT_DEBUG_CLOCK_HZ);
    rc = rk3576_ddr_tag_area_write(area, RK3576_TAG_BOOT_DEFAULTS, &ta50);
    if (rc)
        return rc;

    rk3576_ddr_build_ta52_mem_regions(&ta52, geometry, active_channel_map, sys_sgrf_word0);
    rc = rk3576_ddr_tag_area_write(area, RK3576_TAG_MEM_REGION, &ta52);
    if (rc)
        return rc;

    rk3576_ddr_build_ta58_boot_export(&ta58, boot);
    rc = rk3576_ddr_tag_area_write(area, RK3576_TAG_BOOT_EXPORT, &ta58);
    if (rc)
        return rc;

    rk3576_ddr_build_ta5a_static_info(&ta5a, 0);
    return rk3576_ddr_tag_area_write(area, RK3576_TAG_STATIC_INFO, &ta5a);
}
