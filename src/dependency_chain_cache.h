#ifndef __DEPENDENCY_CC_H__
#define __DEPENDENCY_CC_H__

#include "globals/global_types.h"
#include "op.h"
#include "table_info.h"
#include <stdbool.h>

// =================================================================
// 상수 정의
// =================================================================
#define DEPENDENCY_CHAIN_CACHE_SIZE 1024
#define BLOCK_CACHE_SIZE            1024 // 새로 추가된 블록 캐시 크기
#define MAX_CHAIN_LENGTH            64   // 체인의 최대 길이
#define MAX_LIVE_INS                32   // Live-in 목록의 최대 크기
// =================================================================
// 자료구조 정의
// =================================================================

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

// =================================================================
// Function prototypes (원형) 선언
// =================================================================
void init_dependency_chain_cache(uns proc_id);
void reset_dependency_chain_cache(uns proc_id);
void add_dependency_chain(uns proc_id);
Dependency_Chain_Cache_Entry* get_dependency_chain(uns proc_id, Addr pc);
Dependency_Chain_Cache_Entry* get_dependency_chain_block(uns proc_id, Addr pc);
extern Dependency_Chain_Cache_Entry** dependency_chain_caches;
extern Dependency_Chain_Cache_Entry** block_caches;

#endif // __DEPENDENCY_CHAIN_CACHE_H__