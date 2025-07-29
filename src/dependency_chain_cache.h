#ifndef __DEPENDENCY_CC_H__
#define __DEPENDENCY_CC_H__

#include "globals/global_types.h"
#include "op.h"
#include "table_info.h"
#include <stdbool.h>

#define DEPENDENCY_CHAIN_CACHE_SIZE 1024
#define MAX_CHAIN_LENGTH 64

#define MAX_LIVE_INS 128
// SourceList 구조체에 메모리 주소(addrs) 배열 추가
typedef struct SourceList_struct {
    Reg_Info regs[MAX_LIVE_INS];
    Addr     addrs[MAX_LIVE_INS];
    uns      reg_count;
    uns      addr_count;
} SourceList;

// A single op in the dependency chain
typedef struct Chain_Op_Info_struct {
  Counter op_num;
  Addr    pc;
  uns     op_type;
  Cf_Type  cf_type;
  uns     num_srcs;
  uns     num_dests;
  Reg_Info srcs[MAX_SRCS];
  Reg_Info dests[MAX_DESTS];
  Flag     is_h2p;
  Mem_Type mem_type;
  Addr     va;
  uns      mem_size;
} Chain_Op_Info;

// An entry in the dependency chain cache
typedef struct Dependency_Chain_Cache_Entry_struct {
  Flag          is_valid;
  Addr          h2p_branch_pc;
  Counter       h2p_branch_op_num;
  uns           chain_length;
  Chain_Op_Info chain[MAX_CHAIN_LENGTH];
} Dependency_Chain_Cache_Entry;

// Function prototypes
void init_dependency_chain_cache(uns proc_id);
void reset_dependency_chain_cache(uns proc_id);
void add_dependency_chain(uns proc_id);
// void trace_and_store_by_block(uns proc_id);
// bool remove_addr_from_source_list(SourceList* list, Addr addr);
// void add_addr_to_source_list(SourceList* list, Addr addr);
// void add_reg_to_source_list(SourceList* list, Reg_Info* reg);
// bool remove_reg_from_source_list(SourceList* list, Reg_Info* reg);

#endif // __DEPENDENCY_CHAIN_CACHE_H__