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
        chain_op->va = current_op_in_chain->va;
        chain_op->mem_size = current_op_in_chain->mem_size;
        chain_op->mem_type = current_op_in_chain->mem_type;
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
                chain_op->va = prev_op->va;
                chain_op->mem_size = prev_op->mem_size;
                chain_op->mem_type = prev_op->mem_type;
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

// void add_reg_to_source_list(SourceList* list, Reg_Info* reg) {
//     if (list->reg_count >= MAX_LIVE_INS) return; // List is full

//     // Check for duplicates before adding
//     for (uns i = 0; i < list->reg_count; ++i) {
//         if (list->regs[i].id == reg->id) {
//             return; // Already in the list
//         }
//     }
//     list->regs[list->reg_count++] = *reg;
// }


// bool remove_reg_from_source_list(SourceList* list, Reg_Info* reg) {
//     for (uns i = 0; i < list->reg_count; i++) {
//         if (list->regs[i].id == reg->id) {
//             // Swap with the last element and decrement count
//             list->regs[i] = list->regs[--list->reg_count];
//             return true;
//         }
//     }
//     return false;
// }


// void add_addr_to_source_list(SourceList* list, Addr addr) {
//     if (list->addr_count >= MAX_LIVE_INS) return;
//     for (uns i = 0; i < list->addr_count; ++i) if (list->addrs[i] == addr) return;
//     list->addrs[list->addr_count++] = addr;
// }

// bool remove_addr_from_source_list(SourceList* list, Addr addr) {
//     for (uns i = 0; i < list->addr_count; i++) {
//         if (list->addrs[i] == addr) {
//             list->addrs[i] = list->addrs[--list->addr_count];
//             return true;
//         }
//     }
//     return false;
// }


// void trace_and_store_by_block(uns proc_id) {
//     Fill_Buffer* fb = retired_fill_buffers[proc_id];
//     if (!fb || fb->count == 0) return;

//     typedef struct Marked_Op_Info_struct {
//         Retired_Op_Info op;
//         bool is_marked;
//     } Marked_Op_Info;

//     Marked_Op_Info marked_ops[FILL_BUFFER_SIZE];
//     memset(marked_ops, 0, sizeof(marked_ops));
//     SourceList source_list = {0};

//     // --- 단계 1: 메모리 주소까지 고려하는 Backward Dataflow Walk ---
//     int current_idx = (fb->tail - 1 + fb->size) % fb->size;
//     for (int i = 0; i < fb->count; ++i) {
//         Retired_Op_Info* current_op = &fb->entries[current_idx];
//         marked_ops[i].op = *current_op;

//         if (current_op->hbt_pred_is_hard) {
//             marked_ops[i].is_marked = true;
//             for (int s = 0; s < current_op->num_src_regs; ++s) {
//                 add_reg_to_source_list(&source_list, &current_op->src_reg_id[s]);
//             }
//         }

//         bool is_part_of_chain = false;
//         if (current_op->table_info) {
//             // 목적지 레지스터가 SourceList에 있는지 확인
//             for (int d = 0; d < current_op->num_dest_regs; ++d) {
//                 if (remove_reg_from_source_list(&source_list, &current_op->dst_reg_id[d])) {
//                     is_part_of_chain = true;
//                 }
//             }
//             // 스토어 명령어이고, 목적지 메모리 주소가 SourceList에 있는지 확인
//             if (current_op->table_info->mem_type == MEM_ST && 
//                 remove_addr_from_source_list(&source_list, current_op->va)) {
//                 is_part_of_chain = true;
//             }
//         }
        
//         if (is_part_of_chain) {
//             marked_ops[i].is_marked = true;
//             // 의존성을 해결했으므로, 이 op의 소스들을 새로 SourceList에 추가
//             for (int s = 0; s < current_op->num_src_regs; ++s) {
//                 add_reg_to_source_list(&source_list, &current_op->src_reg_id[s]);
//             }
//             // 만약 로드 명령어라면, 이 값을 생성한 스토어를 찾기 위해 메모리 주소를 SourceList에 추가
//             if (current_op->table_info && current_op->table_info->mem_type == MEM_LD) {
//                 add_addr_to_source_list(&source_list, current_op->va);
//             }
//         }
//         current_idx = (current_idx - 1 + fb->size) % fb->size;
//     }

//    // --- 단계 2: '표시된' 명령어들을 '기본 블록' 단위로 묶어 캐시에 저장 ---
//     // 가장 오래된 명령어부터 순서대로 스캔하여 블록의 시작점을 찾습니다.
//     for (int i = fb->count - 1; i >= 0; --i) {
//         if (!marked_ops[i].is_marked) continue;

//         // '표시된' 블록의 시작점을 찾았음
//         Addr block_pc = marked_ops[i].op.pc;
//         int entry_index = block_pc % DEPENDENCY_CHAIN_CACHE_SIZE;
//         Dependency_Chain_Cache_Entry* entry = &dependency_chain_caches[proc_id][entry_index];

//         entry->is_valid = TRUE;
//         entry->h2p_branch_pc = block_pc; // 블록의 시작 PC로 태깅
//         entry->h2p_branch_op_num = marked_ops[i].op.op_num;
//         entry->chain_length = 0;

//         // 이 블록에 속하는 연속된 '표시된' 명령어들을 모두 캐시에 추가
//         for (int j = i; j >= 0; --j) {
//             if (marked_ops[j].is_marked) {
//                 if (entry->chain_length < MAX_CHAIN_LENGTH) {
//                     Chain_Op_Info* chain_op = &entry->chain[entry->chain_length++];
//                     Retired_Op_Info* source_op = &marked_ops[j].op;

//                     // Copy all relevant info from Retired_Op_Info to Chain_Op_Info
//                     chain_op->op_num = source_op->op_num;
//                     chain_op->pc = source_op->pc;
//                     chain_op->is_h2p = source_op->hbt_pred_is_hard;
//                     if (source_op->table_info) {
//                         chain_op->op_type = source_op->table_info->op_type;
//                         chain_op->cf_type = source_op->cf_type;
//                         chain_op->num_dests = source_op->num_dest_regs;
//                         chain_op->num_srcs = source_op->num_src_regs;
//                         chain_op->va = source_op->va;
//                         chain_op->mem_size = source_op->mem_size;
//                         chain_op->mem_type = source_op->mem_type;
//                         memcpy(chain_op->dests, source_op->dst_reg_id, source_op->num_dest_regs * sizeof(Reg_Info));
//                         memcpy(chain_op->srcs, source_op->src_reg_id, source_op->num_src_regs * sizeof(Reg_Info));
//                     } else {
//                         // Handle cases where table_info might be null
//                         chain_op->op_type = OP_INV;
//                         chain_op->cf_type = NOT_CF;
//                         chain_op->num_dests = 0;
//                         chain_op->num_srcs = 0;
//                     }
//                 }
//                 marked_ops[j].is_marked = false; // 중복 처리를 막기 위해 표시 해제
                
//                 // 만약 다음 명령어가 분기 명령어이면, 현재 기본 블록은 여기서 끝남
//                 if (j > 0 && marked_ops[j - 1].op.table_info &&
//                     marked_ops[j - 1].op.table_info->cf_type != NOT_CF) {
//                     break;
//                 }
//             } else {
//                 // '표시되지 않은' 명령어를 만나면 블록이 끝난 것
//                 break;
//             }
//         }
//         log_dependency_chain_entry(proc_id, entry, cycle_count);
//     }
// }