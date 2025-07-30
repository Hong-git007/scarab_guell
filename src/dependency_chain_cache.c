#include "dependency_chain_cache.h"
#include "fill_buffer.h"
#include "core.param.h"
#include "globals/assert.h"
#include "log/dependency_chain_log.h"
#include <string.h>
#include <stdbool.h>

// 전역 변수 선언: 두 종류의 캐시
Dependency_Chain_Cache_Entry** dependency_chain_caches;
Dependency_Chain_Cache_Entry** block_caches; // 블록 캐시

// =================================================================
// 캐시 초기화 및 리셋 함수 (기존과 동일)
// =================================================================

void init_dependency_chain_cache(uns proc_id) {
    if (!dependency_chain_caches) {
        dependency_chain_caches = (Dependency_Chain_Cache_Entry**)calloc(NUM_CORES, sizeof(Dependency_Chain_Cache_Entry*));
        block_caches = (Dependency_Chain_Cache_Entry**)calloc(NUM_CORES, sizeof(Dependency_Chain_Cache_Entry*));
        ASSERT(0, dependency_chain_caches && block_caches);
    }
    ASSERT(proc_id < NUM_CORES, "proc_id out of bounds\n");

    dependency_chain_caches[proc_id] = (Dependency_Chain_Cache_Entry*)calloc(DEPENDENCY_CHAIN_CACHE_SIZE, sizeof(Dependency_Chain_Cache_Entry));
    block_caches[proc_id] = (Dependency_Chain_Cache_Entry*)calloc(BLOCK_CACHE_SIZE, sizeof(Dependency_Chain_Cache_Entry));
    
    ASSERT(proc_id, dependency_chain_caches[proc_id]);
    ASSERT(proc_id, block_caches[proc_id]);
}

void reset_dependency_chain_cache(uns proc_id) {
    ASSERT(proc_id < NUM_CORES, "proc_id out of bounds\n");
    if (dependency_chain_caches[proc_id]) {
        memset(dependency_chain_caches[proc_id], 0, sizeof(Dependency_Chain_Cache_Entry) * DEPENDENCY_CHAIN_CACHE_SIZE);
    }
    if (block_caches[proc_id]) {
        memset(block_caches[proc_id], 0, sizeof(Dependency_Chain_Cache_Entry) * BLOCK_CACHE_SIZE);
    }
}

// =================================================================
// 헬퍼 함수 (기존과 동일)
// =================================================================

static void add_reg_to_live_in_list(SourceList* list, Reg_Info* reg) {
    if (list->reg_count >= MAX_LIVE_INS) return;
    for (uns i = 0; i < list->reg_count; ++i) {
        if (list->regs[i].id == reg->id) return;
    }
    list->regs[list->reg_count++] = *reg;
}

static bool remove_reg_from_live_in_list(SourceList* list, Reg_Info* reg) {
    for (uns i = 0; i < list->reg_count; i++) {
        if (list->regs[i].id == reg->id) {
            list->regs[i] = list->regs[--list->reg_count];
            return true;
        }
    }
    return false;
}

static void add_addr_to_live_in_list(SourceList* list, Addr addr) {
    if (list->addr_count >= MAX_LIVE_INS) return;
    for (uns i = 0; i < list->addr_count; ++i) if (list->addrs[i] == addr) return;
    list->addrs[list->addr_count++] = addr;
}

static bool remove_addr_from_live_in_list(SourceList* list, Addr addr) {
    for (uns i = 0; i < list->addr_count; i++) {
        if (list->addrs[i] == addr) {
            list->addrs[i] = list->addrs[--list->addr_count];
            return true;
        }
    }
    return false;
}

static void copy_op_to_chain(Chain_Op_Info* dest, Retired_Op_Info* src) {
    dest->op_num = src->op_num;
    dest->pc = src->pc;
    dest->is_h2p = src->hbt_pred_is_hard;
    if (src->table_info && src->inst_info) {
        dest->op_type = src->table_info->op_type;
        dest->cf_type = src->cf_type;
        dest->mem_type = src->mem_type;
        dest->num_dests = src->num_dest_regs;
        dest->num_srcs = src->num_src_regs;
        dest->va = src->va;
        dest->mem_size = src->mem_size;
        memcpy(dest->dests, src->inst_info->dests, src->num_dest_regs * sizeof(Reg_Info));
        memcpy(dest->srcs, src->inst_info->srcs, src->num_src_regs * sizeof(Reg_Info));
    } else {
        dest->op_type = OP_INV;
        dest->num_dests = 0;
        dest->num_srcs = 0;
    }
}

// =================================================================
// 핵심 로직: 두 캐시를 모두 채우는 통합 함수
// =================================================================

void add_dependency_chain(uns proc_id) {
    ASSERT(proc_id < NUM_CORES, "proc_id out of bounds\n");
    Fill_Buffer* fb = retired_fill_buffers[proc_id];
    if (!fb || fb->count < 1) return;

    // --- 파트 0: Fill Buffer를 시간순으로 정렬 및 기본 블록 시작 PC 미리 계산 ---
    Retired_Op_Info ordered_ops[FILL_BUFFER_SIZE];
    int ordered_op_count = fb->count;
    int current_buffer_idx = fb->head;
    for (int i = 0; i < ordered_op_count; ++i) {
        ordered_ops[i] = fb->entries[current_buffer_idx];
        current_buffer_idx = (current_buffer_idx + 1) % fb->size;
    }

    Addr block_start_pc_map[FILL_BUFFER_SIZE];
    Addr current_block_start_pc = 0;
    if (ordered_op_count > 0) {
        current_block_start_pc = ordered_ops[0].pc;
    }
    for (int i = 0; i < ordered_op_count; ++i) {
        block_start_pc_map[i] = current_block_start_pc;
        if (ordered_ops[i].cf_type != NOT_CF) {
            if (i + 1 < ordered_op_count) {
                current_block_start_pc = ordered_ops[i + 1].pc;
            }
        }
    }

    // --- 파트 1: 순수 데이터 의존성 명령어 집합 및 분석 범위 결정 ---
    int trigger_op_idx = -1;
    for (int i = ordered_op_count - 1; i >= 0; --i) {
        if (ordered_ops[i].hbt_pred_is_hard) {
            trigger_op_idx = i;
            break;
        }
    }
    if (trigger_op_idx == -1) return;

    SourceList live_in_list = {0};
    bool is_data_dependent[FILL_BUFFER_SIZE];
    memset(is_data_dependent, 0, sizeof(is_data_dependent));
    
    Retired_Op_Info* trigger_op = &ordered_ops[trigger_op_idx];
    is_data_dependent[trigger_op_idx] = true;

    if (trigger_op->inst_info) {
        for (int i = 0; i < trigger_op->num_src_regs; ++i) add_reg_to_live_in_list(&live_in_list, &trigger_op->inst_info->srcs[i]);
    }
    if (trigger_op->mem_type == MEM_LD) add_addr_to_live_in_list(&live_in_list, trigger_op->va);

    int first_dep_op_idx = trigger_op_idx;
    for (int i = trigger_op_idx - 1; i >= 0; --i) {
        Retired_Op_Info* current_op = &ordered_ops[i];
        bool depends = false;
        if (!current_op->table_info || !current_op->inst_info) continue;
        for (int d = 0; d < current_op->num_dest_regs; ++d) {
            if (remove_reg_from_live_in_list(&live_in_list, &current_op->inst_info->dests[d])) depends = true;
        }
        if (current_op->mem_type == MEM_ST && remove_addr_from_live_in_list(&live_in_list, current_op->va)) {
            depends = true;
        }
        if (depends) {
            is_data_dependent[i] = true;
            first_dep_op_idx = i;
            for (int s = 0; s < current_op->num_src_regs; ++s) add_reg_to_live_in_list(&live_in_list, &current_op->inst_info->srcs[s]);
            if (current_op->mem_type == MEM_LD) add_addr_to_live_in_list(&live_in_list, current_op->va);
        }
    }

    // --- 파트 2: 순수 데이터 의존성 체인을 dependency_chain_cache에 저장 ---
    Dependency_Chain_Cache_Entry* dep_cache = dependency_chain_caches[proc_id];
    int dep_entry_index = trigger_op->pc % DEPENDENCY_CHAIN_CACHE_SIZE;
    Dependency_Chain_Cache_Entry* dep_entry = &dep_cache[dep_entry_index];
    
    dep_entry->is_valid = TRUE;
    dep_entry->h2p_branch_pc = trigger_op->pc;
    dep_entry->h2p_branch_op_num = trigger_op->op_num;
    dep_entry->chain_length = 0;
    for (int i = first_dep_op_idx; i <= trigger_op_idx; ++i) {
        if (is_data_dependent[i]) {
            if (dep_entry->chain_length < MAX_CHAIN_LENGTH) {
                copy_op_to_chain(&dep_entry->chain[dep_entry->chain_length++], &ordered_ops[i]);
            }
        }
    }
    log_dependency_chain_entry(proc_id, dep_entry, cycle_count);
    
    // --- 파트 3: 블록 캐시 저장 로직 (최적화된 단일 패스 방식 + 정확한 Starting PC) ---
    Dependency_Chain_Cache_Entry* block_cache = block_caches[proc_id];
    Chain_Op_Info temp_block_chain[MAX_CHAIN_LENGTH];
    int temp_block_chain_len = 0;

    // 분석 범위(first_dep_op_idx ~ trigger_op_idx)를 단 한번만 순회
    for (int i = first_dep_op_idx; i <= trigger_op_idx; ++i) {
        Retired_Op_Info* current_op = &ordered_ops[i];

        // 단계 1: 현재 명령어가 의존성 체인에 속하면 임시 블록에 추가
        if (is_data_dependent[i]) {
            if (temp_block_chain_len < MAX_CHAIN_LENGTH) {
                copy_op_to_chain(&temp_block_chain[temp_block_chain_len++], current_op);
            }
        }

        // 단계 2: 현재 명령어가 브랜치거나 분석 범위의 마지막이면, 지금까지 쌓인 블록을 저장
        bool is_block_terminator = (current_op->cf_type != NOT_CF);
        bool is_last_op_in_slice = (i == trigger_op_idx);

        if (is_block_terminator || is_last_op_in_slice) {
            if (temp_block_chain_len > 0) {
                // 'Block Starting PC' 문제 해결: 맵을 참조하여 실제 블록 시작 PC를 가져옴
                int first_op_in_block_orig_idx = -1;
                for (int k = 0; k < ordered_op_count; ++k) {
                    if(ordered_ops[k].op_num == temp_block_chain[0].op_num) {
                        first_op_in_block_orig_idx = k;
                        break;
                    }
                }
                
                if (first_op_in_block_orig_idx != -1) {
                    Addr real_block_start_pc = block_start_pc_map[first_op_in_block_orig_idx];
                    int block_entry_index = real_block_start_pc % BLOCK_CACHE_SIZE;
                    Dependency_Chain_Cache_Entry* block_entry = &block_cache[block_entry_index];

                    block_entry->is_valid = TRUE;
                    block_entry->h2p_branch_pc = real_block_start_pc;
                    block_entry->h2p_branch_op_num = temp_block_chain[0].op_num;
                    block_entry->chain_length = temp_block_chain_len;
                    memcpy(block_entry->chain, temp_block_chain, temp_block_chain_len * sizeof(Chain_Op_Info));
                    
                    log_dependency_chain_block(proc_id, block_entry, cycle_count);
                }
            }
            
            // 다음 블록을 위해 임시 체인 초기화
            temp_block_chain_len = 0;
        }
    }
}
// =================================================================
// 캐시 조회 함수 (두 캐시에 대한 각각의 함수)
// =================================================================

// H2P 전체 체인 조회
Dependency_Chain_Cache_Entry* get_dependency_chain(uns proc_id, Addr pc) {
    ASSERT(proc_id < NUM_CORES, "proc_id out of bounds\n");
    int entry_index = pc % DEPENDENCY_CHAIN_CACHE_SIZE;
    Dependency_Chain_Cache_Entry* entry = &dependency_chain_caches[proc_id][entry_index];
    if (entry->is_valid && entry->h2p_branch_pc == pc) {
        return entry;
    }
    return NULL;
}

// 기본 블록 체인 조회
Dependency_Chain_Cache_Entry* get_dependency_chain_block(uns proc_id, Addr pc) {
    ASSERT(proc_id < NUM_CORES, "proc_id out of bounds\n");
    int entry_index = pc % BLOCK_CACHE_SIZE;
    Dependency_Chain_Cache_Entry* entry = &block_caches[proc_id][entry_index];
    if (entry->is_valid && entry->h2p_branch_pc == pc) {
        return entry;
    }
    return NULL;
}