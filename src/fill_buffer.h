#ifndef __FILL_BUFFER_H__
#define __FILL_BUFFER_H__

#include "globals/global_types.h"
#include "op.h" // For Op struct definition
#include "core.param.h"

// A simplified structure to hold retired op information
typedef struct Retired_Op_Info_struct {
  Inst_Info* inst_info;
  Table_Info* table_info;
  Counter op_num;
  Addr    pc;
  Counter issue_cycle;
  Counter done_cycle;
  Counter retire_cycle;
  Flag    hbt_pred_is_hard;
  uns32   hbt_misp_counter;
  uns8    num_src_regs;
  uns8    num_dest_regs;
  int     src_reg_id[MAX_SRCS][REG_TABLE_TYPE_NUM];
  int     dst_reg_id[MAX_DESTS][REG_TABLE_TYPE_NUM];
  // Add other fields as necessary
} Retired_Op_Info;

typedef struct Fill_Buffer_struct {
  Retired_Op_Info* entries;
  int             head;
  int             tail;
  int             count;
  int             size;
  char*           name;
} Fill_Buffer;

extern Fill_Buffer** retired_fill_buffers;

// Function prototypes
void init_fill_buffer(uns proc_id, const char* name);
void fill_buffer_add(uns proc_id, Op* op);
void reset_fill_buffer(uns proc_id);

#endif // __FILL_BUFFER_H__
