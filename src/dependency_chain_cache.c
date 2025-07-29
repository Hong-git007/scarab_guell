#include "dependency_chain_cache.h"
#include "fill_buffer.h"
#include "core.param.h"
#include "globals/assert.h"
#include "log/dependency_chain_log.h"
#include <string.h>
#include <stdbool.h>

static Dependency_Chain_Cache_Entry** dependency_chain_caches;

void init_dependency_chain_cache(uns proc_id) {
    if (!dependency_chain_caches) {
        dependency_chain_caches = (Dependency_Chain_Cache_Entry**)calloc(NUM_CORES, sizeof(Dependency_Chain_Cache_Entry*));
        ASSERT(0, dependency_chain_caches);
    }
    ASSERT(proc_id < NUM_CORES, "proc_id out of bounds\n");
    dependency_chain_caches[proc_id] = (Dependency_Chain_Cache_Entry*)calloc(DEPENDENCY_CHAIN_CACHE_SIZE, sizeof(Dependency_Chain_Cache_Entry));
    ASSERT(proc_id, dependency_chain_caches[proc_id]);
}

void reset_dependency_chain_cache(uns proc_id) {
    ASSERT(proc_id < NUM_CORES, "proc_id out of bounds\n");
    if (dependency_chain_caches[proc_id]) {
        memset(dependency_chain_caches[proc_id], 0, sizeof(Dependency_Chain_Cache_Entry) * DEPENDENCY_CHAIN_CACHE_SIZE);
    }
}

// Helper function to check for logical register dependency
static bool has_logical_dependency(Retired_Op_Info* current_op, Retired_Op_Info* prev_op) {
    if (!current_op->inst_info || !prev_op->inst_info || !current_op->table_info || !prev_op->table_info) return false;

    for (int i = 0; i < current_op->table_info->num_src_regs; ++i) {
        for (int j = 0; j < prev_op->table_info->num_dest_regs; ++j) {
            if (current_op->inst_info->srcs[i].id == prev_op->inst_info->dests[j].id) {
                return true;
            }
        }
    }
    return false;
}

// Refactored function that only depends on proc_id
void add_dependency_chain(uns proc_id) {
    ASSERT(proc_id < NUM_CORES, "proc_id out of bounds\n");
    
    Fill_Buffer* fb = retired_fill_buffers[proc_id];
    if (!fb || fb->count < 1) return;

    int h2p_op_idx = fb->tail;
    Retired_Op_Info* current_op_in_chain = &fb->entries[h2p_op_idx];

    Dependency_Chain_Cache_Entry* cache = dependency_chain_caches[proc_id];
    int entry_index = current_op_in_chain->pc % DEPENDENCY_CHAIN_CACHE_SIZE;
    Dependency_Chain_Cache_Entry* entry = &cache[entry_index];

    entry->is_valid = TRUE;
    entry->h2p_branch_pc = current_op_in_chain->pc;
    entry->h2p_branch_op_num = current_op_in_chain->op_num;
    entry->chain_length = 0;

    Chain_Op_Info* chain_op = &entry->chain[entry->chain_length++];
    chain_op->op_num = current_op_in_chain->op_num;
    chain_op->pc = current_op_in_chain->pc;
    chain_op->is_h2p = current_op_in_chain->hbt_pred_is_hard;

    if (current_op_in_chain->table_info) {
        chain_op->op_type = current_op_in_chain->table_info->op_type;
        chain_op->cf_type = current_op_in_chain->cf_type;
        chain_op->num_dests = current_op_in_chain->num_dest_regs;
        chain_op->num_srcs = current_op_in_chain->num_src_regs;
        memcpy(chain_op->dests, current_op_in_chain->dst_reg_id, current_op_in_chain->num_dest_regs * sizeof(Reg_Info));
        memcpy(chain_op->srcs, current_op_in_chain->src_reg_id, current_op_in_chain->num_src_regs * sizeof(Reg_Info));
    } else {
        chain_op->op_type = OP_INV;
        chain_op->num_dests = 0;
        chain_op->num_srcs = 0;
    }

    if (fb->count < 2) {
        log_dependency_chain_entry(proc_id, entry, cycle_count);
        return;
    }

    int current_idx = (h2p_op_idx - 1 + fb->size) % fb->size;
    for (int i = 0; i < fb->count - 1 && entry->chain_length < MAX_CHAIN_LENGTH; ++i) {
        Retired_Op_Info* prev_op = &fb->entries[current_idx];

        if (has_logical_dependency(current_op_in_chain, prev_op)) {
            chain_op = &entry->chain[entry->chain_length++];
            chain_op->op_num = prev_op->op_num;
            chain_op->pc = prev_op->pc;
            chain_op->is_h2p = prev_op->hbt_pred_is_hard;
            if (prev_op->table_info) {
                chain_op->op_type = prev_op->table_info->op_type;
                chain_op->num_dests = prev_op->num_dest_regs;
                chain_op->num_srcs = prev_op->num_src_regs;
                chain_op->cf_type = prev_op->cf_type;
                memcpy(chain_op->dests, prev_op->dst_reg_id, prev_op->num_dest_regs * sizeof(Reg_Info));
                memcpy(chain_op->srcs, prev_op->src_reg_id, prev_op->num_src_regs * sizeof(Reg_Info));
            } else {
                chain_op->op_type = OP_INV;
                chain_op->num_dests = 0;
                chain_op->num_srcs = 0;
            }
            current_op_in_chain = prev_op;
        }
        current_idx = (current_idx - 1 + fb->size) % fb->size;
    }

    for(int i = 0; i < entry->chain_length / 2; ++i) {
        Chain_Op_Info temp = entry->chain[i];
        entry->chain[i] = entry->chain[entry->chain_length - 1 - i];
        entry->chain[entry->chain_length - 1 - i] = temp;
    }

    log_dependency_chain_entry(proc_id, entry, cycle_count);
}