#include "rk3576_ddr/training.h"

#include "rk3576_ddr/types.h"

enum {
    RK3576_DDR_SINGLE_RANK_SELECTOR = 0u,
    RK3576_DDR_DUAL_RANK_COUNT = 2u,
    RK3576_DDR_DUAL_RANK_SELECTOR = 3u,
    RK3576_DDR_TTOT_ENABLED = 1u,
};

static void append_training_pass(rk3576_ddr_training_pass *passes, size_t capacity, size_t *count,
                                 rk3576_ddr_channel_id channel, uint32_t rank_selector,
                                 uint32_t rank_slot, rk3576_ddr_fsp_index fsp_index,
                                 rk3576_ddr_training_mode mode, uint32_t ttot_enabled, uint32_t flags)
{
    if (passes && *count < capacity) {
        passes[*count].channel = channel;
        passes[*count].rank_selector = rank_selector;
        passes[*count].rank_slot = rank_slot;
        passes[*count].fsp_index = fsp_index;
        passes[*count].mode = mode;
        passes[*count].ttot_enabled = ttot_enabled;
        passes[*count].flags = flags;
    }
    ++*count;
}

size_t rk3576_ddr_plan_dvfs_training_passes(rk3576_ddr_training_pass *passes, size_t capacity,
                                             const uint32_t active_channel_map[RK3576_DDR_MAX_CHANNELS],
                                             const uint32_t rank_count_by_channel[RK3576_DDR_MAX_CHANNELS],
                                             uint32_t rank_slot, rk3576_ddr_fsp_index fsp_index)
{
    static const rk3576_ddr_training_mode modes[] = {
        RK3576_DDR_TRAINING_TTOT_MR12,
        RK3576_DDR_TRAINING_TTOT_MR12,
        RK3576_DDR_TRAINING_DQ_PATTERN,
        RK3576_DDR_TRAINING_DQ_PATTERN,
    };
    static const uint32_t flags[] = {
        RK3576_DDR_TRAINING_FLAG_ENTRY_SETUP,
        RK3576_DDR_TRAINING_FLAG_RUN,
        RK3576_DDR_TRAINING_FLAG_NONE,
        RK3576_DDR_TRAINING_FLAG_ENTRY_SETUP | RK3576_DDR_TRAINING_FLAG_EXIT_RESTORE | RK3576_DDR_TRAINING_FLAG_RUN,
    };
    size_t count = 0;

    if (!active_channel_map || !rank_count_by_channel)
        return 0;

    for (size_t phase = 0; phase < sizeof(modes) / sizeof(modes[0]); ++phase) {
        for (uint32_t slot = 0; slot < RK3576_DDR_MAX_CHANNELS; ++slot) {
            rk3576_ddr_channel_id channel = active_channel_map[slot];
            uint32_t rank_selector;

            if (!rk3576_ddr_channel_is_active(channel))
                continue;

            rank_selector = rank_count_by_channel[channel] == RK3576_DDR_DUAL_RANK_COUNT ? RK3576_DDR_DUAL_RANK_SELECTOR : RK3576_DDR_SINGLE_RANK_SELECTOR;
            append_training_pass(passes, capacity, &count, channel, rank_selector, rank_slot,
                                 fsp_index, modes[phase], RK3576_DDR_TTOT_ENABLED, flags[phase]);
        }
    }

    return count;
}
