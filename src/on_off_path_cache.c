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

void record_on_off_path(uns proc_id, Retired_Op_Info* h2p_op_at_head) {
    Fill_Buffer* fb = retired_fill_buffers[proc_id];
    if (!fb || fb->count == 0) return;

    On_Off_Path_Cache_Entry* cache = on_off_path_caches[proc_id];
    int entry_index = h2p_op_at_head->pc % ON_OFF_PATH_CACHE_SIZE;
    On_Off_Path_Cache_Entry* entry = &cache[entry_index];

    entry->is_valid = TRUE;
    entry->h2p_branch_pc = h2p_op_at_head->pc;
    entry->h2p_branch_op_num = h2p_op_at_head->op_num;
    entry->path_length = 0;

    int current_idx = fb->head;
    for (int i = 0; i < fb->count && entry->path_length < MAX_ON_OFF_PATH_LENGTH; ++i) {
        Retired_Op_Info* op_in_path = &fb->entries[current_idx];
        
        Path_Op_Info* path_op = &entry->path[entry->path_length++];
        path_op->op_num = op_in_path->op_num;
        path_op->pc = op_in_path->pc;
        path_op->sched_cycle = op_in_path->sched_cycle;
        path_op->exec_cycle = op_in_path->exec_cycle;
        path_op->done_cycle = op_in_path->done_cycle;
        path_op->retire_cycle = op_in_path->retire_cycle;
        if (op_in_path->table_info) {
            path_op->op_type = op_in_path->table_info->op_type;
            path_op->cf_type = op_in_path->cf_type;
            path_op->num_srcs = op_in_path->num_src_regs;
            path_op->num_dests = op_in_path->num_dest_regs;
            memcpy(path_op->srcs, op_in_path->src_reg_id, op_in_path->num_src_regs * sizeof(Reg_Info));
            memcpy(path_op->dests, op_in_path->dst_reg_id, op_in_path->num_dest_regs * sizeof(Reg_Info));
            path_op->is_h2p = op_in_path->hbt_pred_is_hard;
            path_op->mem_type = op_in_path->mem_type;
            path_op->va = op_in_path->va;
            path_op->mem_size = op_in_path->mem_size;
        } else {
            path_op->op_type = OP_INV;
            path_op->num_srcs = 0;
            path_op->num_dests = 0;
        }

        current_idx = (current_idx + 1) % fb->size;
    }
    log_on_off_path_entry(proc_id, entry, cycle_count);
}