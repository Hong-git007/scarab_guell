/*
 * Copyright 2025 University of California Santa Cruz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/***************************************************************************************
 * File         : node_issue_queue.cc
 * Author       : Yinyuan Zhao, Litz Lab
 * Date         : 4/15/2025
 * Description  :
 ***************************************************************************************/

#include "node_issue_queue.h"

extern "C" {
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"

#include "debug/debug.param.h"
#include "debug/debug_macros.h"
#include "debug/debug_print.h"

#include "memory/memory.h"

#include "exec_ports.h"
#include "node_stage.h"
}

/**************************************************************************************/
/* Macros */

#define DEBUG_NODE(proc_id, args...) _DEBUG(proc_id, DEBUG_NODE_STAGE, ##args)
#define DEBUG_IQ(proc_id, args...)   _DEBUG(proc_id, DEBUG_ISSUE_QUEUE, ##args)

/**************************************************************************************/
/* Prototypes */

int64 node_dispatch_find_emptiest_rs(Op*);
void node_schedule_oldest_first_sched(Op*);

const char* debug_print_rs_mask(Reservation_Station* rs) {
  // RS가 유효하지 않거나, entry_status가 할당되지 않았거나, 크기가 0이면 기본값을 반환합니다.
  if (!rs || !rs->entry_status || rs->size == 0) {
    return "TAIL < ... > HEAD";
  }

  static char buffer[1024]; 
  char* ptr = buffer;
  size_t remaining = sizeof(buffer);

  // 접두사를 추가합니다.
  int written = snprintf(ptr, remaining, "TAIL< ");
  if (written > 0) {
      ptr += written;
      remaining -= written;
  }
  
  // TAIL(rs->size - 1)부터 HEAD(0)까지 슬롯 인덱스를 역순으로 순회합니다.
  for (int abs_slot_id = rs->size - 1; abs_slot_id >= 0; --abs_slot_id) {
    if (remaining <= 1) break;

    // 현재 슬롯 인덱스(abs_slot_id)를 통해 청크 번호와 청크 내 비트 위치를 계산합니다.
    size_t chunk_index = abs_slot_id / 64;
    int bit_pos = abs_slot_id % 64;
    uint64_t chunk = rs->entry_status[chunk_index];

    // 해당 비트의 상태('1' 또는 '0')를 버퍼에 씁니다.
    if ((chunk >> bit_pos) & 1) {
      *ptr = '1';
    } else {
      *ptr = '0';
    }
    ptr++;
    remaining--;

    // 청크 경계에 도달하면 가독성을 위해 공백을 추가합니다. (맨 오른쪽 제외)
    if (abs_slot_id > 0 && abs_slot_id % 64 == 0) {
      if (remaining > 1) {
        *ptr++ = ' ';
        remaining--;
      }
    }
  }

  // 접미사를 추가합니다.
  snprintf(ptr, remaining, " >HEAD");
  
  return buffer;
}

/**************************************************************************************/
/* Issuers:
 *      The interface to the issue functions is that Scarab will pass the
 * function the op to be issued, and the issuer will return the RS id that the
 * op should be issued to, or -1 meaning that there is no RS for the op to
 * be issued to.
 */

/*
 * FIND_EMPTIEST_RS: will always select the RS with the most empty slots
 */
int64 node_dispatch_find_emptiest_rs(Op* op) {
  int64 emptiest_rs_id = NODE_ISSUE_QUEUE_RS_SLOT_INVALID;
  uns emptiest_rs_slots = 0;

  /*
   * Iterate through RSs looking for an available RS that is connected to
   * an FU that can execute the OP.
   */
  for (int64 rs_id = 0; rs_id < NUM_RS; ++rs_id) {
    Reservation_Station* rs = &node->rs[rs_id];
    ASSERT(node->proc_id, !rs->size || rs->rs_op_count <= rs->size);

    /* TODO: support infinite RS for upper-bound expr */
    ASSERTM(node->proc_id, rs->size, "Infinite RS not suppoted by node_dispatch_find_emptiest_rs issuer.");

    for (uns32 i = 0; i < rs->num_fus; ++i) {
      // find the FU that can execute this op
      Func_Unit* fu = rs->connected_fus[i];
      if (!(get_fu_type(op->table_info->op_type, op->table_info->is_simd) & fu->type)) {
        continue;
      }

      ASSERT(node->proc_id, rs->size >= rs->rs_op_count);
      uns num_empty_slots = rs->size - rs->rs_op_count;
      if (num_empty_slots == 0) {
        continue;
      }

      // find the emptiest RS
      if (emptiest_rs_slots < num_empty_slots) {
        emptiest_rs_id = rs_id;
        emptiest_rs_slots = num_empty_slots;
      }
    }
  }

  return emptiest_rs_id;
}

/**************************************************************************************/
/* Schedulers:
 *      The interface to the schedule functions is that Scarab will pass the
 * function the ready op, and the scheduler will return the selected ops in
 * node->sd. See OLDEST_FIRST_SCHED for an example. Note, it is not necessary
 * to look at FU availability in this stage, if the FU is busy, then the op
 * will be ignored and available to schedule again in the next stage.
 */

/*
 * OLDEST_FIRST_SCHED: will always select the oldest ready ops to schedule
 */
void node_schedule_oldest_first_sched(Op* op) {
  int32 youngest_slot_op_id = NODE_ISSUE_QUEUE_FU_SLOT_INVALID;

  // Iterate through the FUs that this RS is connected to.
  Reservation_Station* rs = &node->rs[op->rs_id];
  for (uns32 i = 0; i < rs->num_fus; ++i) {
    // check if this op can be executed by this FU
    Func_Unit* fu = rs->connected_fus[i];
    if (!(get_fu_type(op->table_info->op_type, op->table_info->is_simd) & fu->type)) {
      continue;
    }

    uns32 fu_id = fu->fu_id;
    Op* s_op = node->sd.ops[fu_id];

    // nobody has been scheduled to this FU yet
    if (!s_op) {
      DEBUG_NODE(node->proc_id, "Scheduler selecting    op_num:%s  fu_id:%d op:%s l1:%d\n", unsstr64(op->op_num), fu_id,
            disasm_op(op, TRUE), op->engine_info.l1_miss);
      ASSERT(node->proc_id, fu_id < (uns32)node->sd.max_op_count);
      op->fu_num = fu_id;
      node->sd.ops[op->fu_num] = op;
      node->last_scheduled_opnum = op->op_num;
      node->sd.op_count += !s_op;
      ASSERT(node->proc_id, node->sd.op_count <= node->sd.max_op_count);
      return;
    }

    if (op->op_num >= s_op->op_num) {
      continue;
    }

    // The slot is not empty, but we are older than the op that is in the slot
    if (youngest_slot_op_id == NODE_ISSUE_QUEUE_FU_SLOT_INVALID) {
      youngest_slot_op_id = fu_id;
      continue;
    }

    // check if this slot is younger than the youngest known op
    Op* youngest_op = node->sd.ops[youngest_slot_op_id];
    if (s_op->op_num > youngest_op->op_num) {
      youngest_slot_op_id = fu_id;
    }
  }

  /* Did not find an empty slot or a slot that is younger than me, do nothing */
  if (youngest_slot_op_id == NODE_ISSUE_QUEUE_FU_SLOT_INVALID) {
    return;
  }

  /* Did not find an empty slot, but we did find a slot that is younger that us */
  uns32 fu_id = youngest_slot_op_id;
  DEBUG_NODE(node->proc_id, "Scheduler selecting    op_num:%s  fu_id:%d op:%s l1:%d\n", unsstr64(op->op_num), fu_id,
        disasm_op(op, TRUE), op->engine_info.l1_miss);
  ASSERT(node->proc_id, fu_id < (uns32)node->sd.max_op_count);
  op->fu_num = fu_id;
  node->sd.ops[op->fu_num] = op;
  node->last_scheduled_opnum = op->op_num;
  node->sd.op_count += 0;  // replacing an op, not adding a new one.
  ASSERT(node->proc_id, node->sd.op_count <= node->sd.max_op_count);
}

/**************************************************************************************/
/* Driven Table */

using Dispatch_Func = int64 (*)(Op*);
Dispatch_Func dispatch_func_table[NODE_ISSUE_QUEUE_DISPATCH_SCHEME_NUM] = {
    [NODE_ISSUE_QUEUE_DISPATCH_SCHEME_FIND_EMPTIEST_RS] = {node_dispatch_find_emptiest_rs},
};

using Schedule_Func = void (*)(Op*);
Schedule_Func schedule_func_table[NODE_ISSUE_QUEUE_SCHEDULE_SCHEME_NUM] = {
    [NODE_ISSUE_QUEUE_SCHEDULE_SCHEME_OLDEST_FIRST] = {node_schedule_oldest_first_sched},
};

/**************************************************************************************/

/*
 * Memory is blocked when there are no more MSHRs in the L1 Q
 * (i.e., there is no way to handle a D-Cache miss).
 * This function checks to see if any of the L1 MSHRs have become available.
 */
void node_issue_queue_check_mem() {
  /* if we are stalled due to lack of MSHRs to the L1, check to see if there is space now. */
  if (node->mem_blocked && mem_can_allocate_req_buffer(node->proc_id, MRT_DFETCH, FALSE)) {
    node->mem_blocked = FALSE;
    STAT_EVENT(node->proc_id, MEM_BLOCK_LENGTH_0 + MIN2(node->mem_block_length, 5000) / 100);
    if (DIE_ON_MEM_BLOCK_THRESH) {
      if (node->proc_id == DIE_ON_MEM_BLOCK_CORE) {
        ASSERTM(node->proc_id, node->mem_block_length < DIE_ON_MEM_BLOCK_THRESH,
                "Core blocked on memory for %u cycles (%llu--%llu)\n", node->mem_block_length,
                cycle_count - node->mem_block_length, cycle_count);
      }
    }
    node->mem_block_length = 0;
  }
  INC_STAT_EVENT(node->proc_id, CORE_MEM_BLOCKED, node->mem_blocked);
  node->mem_block_length += node->mem_blocked;
}

/*
 * Remove scheduled ops (i.e., going from RS to FUs) from the RS and ready queue
 */
void node_issue_queue_clear() {
  // TODO: make this traversal more efficient since we know what ops we tried to schedule last cycle
  Op** last = &node->rdy_head;
  for (Op* op = node->rdy_head; op; op = op->next_rdy) {
    if (op->state != OS_SCHEDULED && op->state != OS_MISS) {
      last = &op->next_rdy;
      continue;
    }

    DEBUG_NODE(node->proc_id, "Removing from RS (and ready list)  op_num:%s op:%s l1:%d\n", unsstr64(op->op_num),
          disasm_op(op, TRUE), op->engine_info.l1_miss);
    *last = op->next_rdy;
    op->in_rdy_list = FALSE;
    
    Reservation_Station* rs = &node->rs[op->rs_id];
    ASSERT(node->proc_id, rs->rs_op_count > 0);
    rs->rs_op_count--;

    // Clear the entry in the RS bitmask
    if (rs->size > 0) {
        size_t chunk_index = op->rs_entry_id / 64;
        size_t bit_pos = op->rs_entry_id % 64;
        ASSERTM(node->proc_id, (rs->entry_status[chunk_index] >> bit_pos) & 1, "Clearing an already free RS entry. op_num:%s rs_id:%llu rs_entry_id:%llu\n", unsstr64(op->op_num), op->rs_id, op->rs_entry_id);
        rs->entry_status[chunk_index] &= ~(1ULL << bit_pos);
        DEBUG_IQ(node->proc_id, "Clearing op_num:%s from rs_id:%llu, rs_entry_id:%llu op:%-80s mask:%s\n", unsstr64(op->op_num), op->rs_id, op->rs_entry_id, disasm_op(op, TRUE), debug_print_rs_mask(rs));
    }

    STAT_EVENT(node->proc_id, OP_ISSUED);
  }
}

/*
 * Fill the scheduling window (RS) with oldest available ops.
 * For each available op:
 *  - Allocate it to its designated reservation station.
 *  - If all source operands are ready, insert it into the ready list.
 */
void node_issue_queue_dispatch() {
  Op* op = NULL;
  uns32 num_fill_rs = 0;

  /* Scan through dispatched nodes in node table that have not been filled to RS yet. */
  for (op = node->next_op_into_rs; op; op = op->next_node) {
    int64 rs_id = dispatch_func_table[NODE_ISSUE_QUEUE_DISPATCH_SCHEME](op);
    if (rs_id == NODE_ISSUE_QUEUE_RS_SLOT_INVALID)
      break;
    ASSERT(node->proc_id, rs_id >= 0 && rs_id < NUM_RS);

    Reservation_Station* rs = &node->rs[rs_id];
    ASSERTM(node->proc_id, !rs->size || rs->rs_op_count < rs->size,
            "There must be at least one free space in selected RS!\n");

    ASSERT(node->proc_id, op->state == OS_IN_ROB);
    op->state = OS_IN_RS;
    op->rs_id = (Counter)rs_id;

    // Find an empty entry in the RS using the bitmask
    int64_t rs_entry_id = -1;
    if (rs->size > 0) {
        size_t num_chunks = (rs->size + 63) / 64;
        for (size_t i = 0; i < num_chunks; ++i) {
            if (rs->entry_status[i] != UINT64_MAX) { // If the chunk is not full
                int free_bit_pos = __builtin_ffsll(~rs->entry_status[i]) - 1;
                rs_entry_id = i * 64 + free_bit_pos;
                if (rs_entry_id < rs->size) {
                    rs->entry_status[i] |= (1ULL << free_bit_pos); // Mark as occupied
                    break;
                } else {
                    rs_entry_id = -1; // Invalid entry
                }
            }
        }
    }
    ASSERTM(node->proc_id, rs_entry_id != -1, "Could not find a free entry in RS %lld, but it was not full.", rs_id);
    op->rs_entry_id = rs_entry_id;

    DEBUG_IQ(node->proc_id, "Dispatching op_num:%s to rs_id:%llu, rs_entry_id:%llu op:%-80s mask:%s\n", unsstr64(op->op_num), op->rs_id, op->rs_entry_id, disasm_op(op, TRUE), debug_print_rs_mask(rs));

    rs->rs_op_count++;
    num_fill_rs++;

    DEBUG_NODE(node->proc_id, "Filling %s with op_num:%s (%d)\n", rs->name, unsstr64(op->op_num), rs->rs_op_count);

    if (op->srcs_not_rdy_vector == 0) {
      /* op is ready to issue right now */
      DEBUG_NODE(node->proc_id, "Adding to ready list  op_num:%s op:%s l1:%d\n", unsstr64(op->op_num), disasm_op(op, TRUE),
            op->engine_info.l1_miss);
      op->state = (cycle_count + 1 >= op->rdy_cycle ? OS_READY : OS_WAIT_FWD);
      op->next_rdy = node->rdy_head;
      node->rdy_head = op;
      op->in_rdy_list = TRUE;
    }

    // maximum number of operations to fill into the RS per cycle (0 = unlimited)
    if (RS_FILL_WIDTH && (num_fill_rs == RS_FILL_WIDTH)) {
      op = op->next_node;
      break;
    }
  }

  // mark the next node to continue filling in the next cycle
  node->next_op_into_rs = op;
}

/*
 * Schedule ready ops (ops that are currently in the ready list).
 *
 * Input:  node->rdy_head, containing all ops that are ready to issue from each of the RSs.
 * Output: node->sd, containing ops thats being passed to the FUs.
 *
 * If a functional unit is available, it will accept the scheduled operation,
 * which is then removed from the ready list. If no FU is available, the
 * operation remains in the ready list to be considered in the next
 * scheduling cycle.
 */
void node_issue_queue_schedule() {
  /*
   * the next stage is supposed to clear them out,
   * regardless of whether they are actually sent to a functional unit
   */
  ASSERT(node->proc_id, node->sd.op_count == 0);

  // Check to see if the L1 Q is (still) full
  node_issue_queue_check_mem();

  for (Op* op = node->rdy_head; op; op = op->next_rdy) {
    ASSERT(node->proc_id, node->proc_id == op->proc_id);
    ASSERTM(node->proc_id, op->in_rdy_list, "op_num %llu\n", op->op_num);
    if (op->state == OS_WAIT_MEM) {
      if (node->mem_blocked)
        continue;
      else
        op->state = OS_READY;
    }

    if (op->state == OS_TENTATIVE || op->state == OS_WAIT_DCACHE)
      continue;

    ASSERTM(node->proc_id, op->state == OS_IN_RS || op->state == OS_READY || op->state == OS_WAIT_FWD,
            "op_num: %llu, op_state: %s\n", op->op_num, Op_State_str(op->state));
    DEBUG_NODE(node->proc_id, "Scheduler examining    op_num:%s op:%s l1:%d st:%s rdy:%s exec:%s done:%s\n",
          unsstr64(op->op_num), disasm_op(op, TRUE), op->engine_info.l1_miss, Op_State_str(op->state),
          unsstr64(op->rdy_cycle), unsstr64(op->exec_cycle), unsstr64(op->done_cycle));

    /* op will be ready next cycle, try to schedule */
    if (cycle_count >= op->rdy_cycle - 1) {
      ASSERT(node->proc_id, op->srcs_not_rdy_vector == 0x0);
      DEBUG_NODE(node->proc_id, "Scheduler considering  op_num:%s op:%s l1:%d\n", unsstr64(op->op_num), disasm_op(op, TRUE),
            op->engine_info.l1_miss);

      schedule_func_table[NODE_ISSUE_QUEUE_SCHEDULE_SCHEME](op);
    }
  }
}

/**************************************************************************************/
/* External Function */

void node_issue_queue_update() {
  /* remove scheduled ops from RS and ready list */
  node_issue_queue_clear();

  /* fill RS with oldest ops waiting for it */
  node_issue_queue_dispatch();

  /* first schedule 1 ready op per NUM_FUS  */
  node_issue_queue_schedule();
}
