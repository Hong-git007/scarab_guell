#include "fill_buffer.h"
#include <stdlib.h>
#include <string.h>
#include "globals/assert.h"
#include "core.param.h"
#include "dependency_chain_cache.h"
#include "on_off_path_cache.h"

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
        // -----[ 새로운 로직 추가 시작 ]-----
        // 1. 덮어씌워질, 즉 가장 오래된 op(head 위치)를 가져옵니다.
        Retired_Op_Info* evicted_op = &fb->entries[fb->head];
        
        // 2. 만약 이 op가 H2P 브랜치라면, 경로 기록 함수를 호출합니다.
        if (evicted_op->hbt_pred_is_hard) {
            record_on_off_path(proc_id, evicted_op);
        }
        // -----[ 새로운 로직 추가 끝 ]-----

        // 기존의 head 포인터 업데이트 로직
        fb->head = (fb->head + 1) % fb->size;
        fb->count--;
    }

    Retired_Op_Info* entry = &fb->entries[fb->tail];
    entry->inst_info = op->inst_info;
    entry->table_info = op->table_info;
    entry->op_num = op->op_num;
    entry->pc = op->inst_info->addr;
    entry->sched_cycle = op->sched_cycle;
    entry->exec_cycle = op->exec_cycle;
    entry->done_cycle = op->done_cycle;
    entry->retire_cycle = op->retire_cycle;
    entry->hbt_pred_is_hard = op->oracle_info.hbt_pred_is_hard;
    entry->hbt_misp_counter = op->oracle_info.hbt_misp_counter;

    if (op->table_info) {
        entry->num_src_regs = op->table_info->num_src_regs;
        entry->num_dest_regs = op->table_info->num_dest_regs;
        entry->cf_type = op->table_info->cf_type;
        ASSERT(proc_id, entry->num_src_regs <= MAX_SRCS);
        ASSERT(proc_id, entry->num_dest_regs <= MAX_DESTS);
        memcpy(entry->src_reg_id, op->inst_info->srcs, op->table_info->num_src_regs * sizeof(Reg_Info));
        memcpy(entry->dst_reg_id, op->inst_info->dests, op->table_info->num_dest_regs * sizeof(Reg_Info));
        if (op->table_info->mem_type != NOT_MEM && op->oracle_info.mem_size > 0) {
        entry->va = op->oracle_info.va;
        entry->mem_size = op->oracle_info.mem_size;
        entry->mem_type = op->table_info->mem_type;
        } else {
        entry->va = 0; // 0 또는 유효하지 않음을 나타내는 값으로 초기화
        entry->mem_size = 0;
        entry->mem_type = NOT_MEM;
        }
    } else {
        entry->num_src_regs = 0;
        entry->num_dest_regs = 0;
    }
    if (entry->hbt_pred_is_hard) {
        add_dependency_chain(proc_id);
    }

    fb->tail = (fb->tail + 1) % fb->size;
    fb->count++;
}
