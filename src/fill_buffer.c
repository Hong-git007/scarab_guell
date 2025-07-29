#include "fill_buffer.h"
#include <stdlib.h>
#include <string.h>
#include "globals/assert.h"
#include "core.param.h"
#include "dependency_chain_cache.h"

Fill_Buffer** retired_fill_buffers;

void init_fill_buffer(uns proc_id, const char* name) {
    ASSERT(proc_id < NUM_CORES, "proc_id out of bounds\n");
    if (!retired_fill_buffers) {
        retired_fill_buffers = (Fill_Buffer**)calloc(NUM_CORES, sizeof(Fill_Buffer*));
        ASSERT(0, retired_fill_buffers);
    }

    retired_fill_buffers[proc_id] = (Fill_Buffer*)malloc(sizeof(Fill_Buffer));
    ASSERT(proc_id, retired_fill_buffers[proc_id]);
    Fill_Buffer* fb = retired_fill_buffers[proc_id];
    
    fb->name = strdup(name);
    fb->size = FILL_BUFFER_SIZE;
    fb->entries = (Retired_Op_Info*)calloc(fb->size, sizeof(Retired_Op_Info));
    ASSERT(proc_id, fb->entries);
    reset_fill_buffer(proc_id);
}

void reset_fill_buffer(uns proc_id) {
    ASSERT(proc_id < NUM_CORES, "proc_id out of bounds\n");
    Fill_Buffer* fb = retired_fill_buffers[proc_id];
    if (fb) {
        fb->head = 0;
        fb->tail = 0;
        fb->count = 0;
        memset(fb->entries, 0, sizeof(Retired_Op_Info) * fb->size);
    }
}

void fill_buffer_add(uns proc_id, Op* op) {
    ASSERT(proc_id < NUM_CORES, "proc_id out of bounds\n");
    Fill_Buffer* fb = retired_fill_buffers[proc_id];
    if (!fb) return;

    // Buffer is full, overwrite the oldest entry
    if (fb->count == fb->size) {
        fb->head = (fb->head + 1) % fb->size;
        fb->count--;
    }

    Retired_Op_Info* entry = &fb->entries[fb->tail];
    entry->inst_info = op->inst_info;
    entry->table_info = op->table_info;
    entry->op_num = op->op_num;
    entry->pc = op->inst_info->addr;
    entry->issue_cycle = op->issue_cycle;
    entry->done_cycle = op->done_cycle;
    entry->retire_cycle = op->retire_cycle;
    entry->hbt_pred_is_hard = op->oracle_info.hbt_pred_is_hard;
    entry->hbt_misp_counter = op->oracle_info.hbt_misp_counter;
    entry->num_src_regs = op->table_info->num_src_regs;
    entry->num_dest_regs = op->table_info->num_dest_regs;
    memcpy(entry->src_reg_id, op->src_reg_id, sizeof(op->src_reg_id));
    memcpy(entry->dst_reg_id, op->dst_reg_id, sizeof(op->dst_reg_id));

    if (entry->hbt_pred_is_hard) {
        add_dependency_chain(proc_id, op);
    }

    fb->tail = (fb->tail + 1) % fb->size;
    fb->count++;
}
