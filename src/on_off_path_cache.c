#include "on_off_path_cache.h"
#include "log/on_off_path_log.h"
#include "fill_buffer.h"
#include "core.param.h"
#include "globals/assert.h"
#include <string.h>
#include <stdbool.h>

static On_Off_Path_Cache_Entry** on_off_path_caches;

void init_on_off_path_cache(uns proc_id) {
    if (!on_off_path_caches) {
        on_off_path_caches = (On_Off_Path_Cache_Entry**)calloc(NUM_CORES, sizeof(On_Off_Path_Cache_Entry*));
        ASSERT(0, on_off_path_caches);
    }
    on_off_path_caches[proc_id] = (On_Off_Path_Cache_Entry*)calloc(ON_OFF_PATH_CACHE_SIZE, sizeof(On_Off_Path_Cache_Entry));
    ASSERT(proc_id, on_off_path_caches[proc_id]);
}

void reset_on_off_path_cache(uns proc_id) {
    if (on_off_path_caches[proc_id]) {
        memset(on_off_path_caches[proc_id], 0, sizeof(On_Off_Path_Cache_Entry) * ON_OFF_PATH_CACHE_SIZE);
    }
}

void record_on_off_path(uns proc_id, Op* h2p_op_at_head) {
    Fill_Buffer* fb = retired_fill_buffers[proc_id];
    if (!fb || fb->count == 0) return;

    On_Off_Path_Cache_Entry* cache = on_off_path_caches[proc_id];
    int entry_index = h2p_op_at_head->inst_info->addr % ON_OFF_PATH_CACHE_SIZE;
    On_Off_Path_Cache_Entry* entry = &cache[entry_index];

    entry->is_valid = TRUE;
    entry->h2p_branch_pc = h2p_op_at_head->inst_info->addr;
    entry->h2p_branch_op_num = h2p_op_at_head->op_num;
    entry->path_length = 0;

    int current_idx = fb->head;
    for (int i = 0; i < fb->count && entry->path_length < MAX_ON_OFF_PATH_LENGTH; ++i) {
        Op* op_in_path = &fb->entries[current_idx];
        entry->path[entry->path_length++] = *op_in_path;
        current_idx = (current_idx + 1) % fb->size;
    }
    log_on_off_path_entry(proc_id, entry, cycle_count);
}
