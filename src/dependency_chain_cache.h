#ifndef __DEPENDENCY_CC_H__
#define __DEPENDENCY_CC_H__

#include "globals/global_types.h"
#include "op.h"
#include "table_info.h"

#define DEPENDENCY_CHAIN_CACHE_SIZE 1024
#define MAX_CHAIN_LENGTH 64

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

#endif // __DEPENDENCY_CHAIN_CACHE_H__
