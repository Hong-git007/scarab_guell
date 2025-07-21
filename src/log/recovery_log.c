#include <stdio.h>
#include <stdlib.h>
#include "recovery_log.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"
#include "op.h"
#include "debug/debug.param.h"
#include "op_trace_log.h" // [추가] 공유 로그 파일 포인터를 사용하기 위해 헤더를 포함합니다.
#include "bp/bp_conf.h"

// 이 파일 전용 로그(recovery_log)를 위한 포인터입니다.
static FILE* recovery_log_file = NULL;
extern char* OUTPUT_DIR;

// 이 파일 전용 로그 파일을 닫는 함수입니다.
static void close_recovery_log_file(void) {
    if (recovery_log_file) {
        fclose(recovery_log_file);
        recovery_log_file = NULL;
    }
}

// 이 파일 전용 로그 파일을 초기화하는 함수입니다.
void init_recovery_log(void) {
    if (recovery_log_file == NULL) {
        recovery_log_file = file_tag_fopen(OUTPUT_DIR, "recovery_log", "w");
        if (recovery_log_file == NULL) {
            perror("Error opening recovery_log in output directory");
        } else {
            atexit(close_recovery_log_file);
        }
    }
    if (local_bpc_data_ptr == NULL) {
        local_bpc_data_ptr = get_bpc_data();
    }
}

void log_misprediction(Op* op, Node_Stage* node, Counter cycle_count, Bp_Recovery_Info* bp_recovery_info) {
    if ((!recovery_log_file && !retired_op_log_file) || cycle_count < DEBUG_CYCLE_START || cycle_count > DEBUG_CYCLE_STOP) return;

    Addr pc_val = op->inst_info->addr;
    int done_before = 0, pending_before = 0, done_after = 0, pending_after = 0;
    int rs_off_path_count = 0, rs_on_path_count = 0;
    int miss_off_path_count = 0, miss_on_path_count = 0;
    Flag op_found = FALSE;

    Op* current_op = node->node_head;
    while(current_op) {
        if (current_op->state == OS_IN_RS) {
            if (current_op->off_path) rs_off_path_count++;
            else rs_on_path_count++;
        } else if (current_op->state == OS_MISS) {
            if (current_op->off_path) miss_off_path_count++;
            else miss_on_path_count++;
        }

        if (current_op == op) {
            op_found = TRUE;
        } else {
            if (op_found) {
                if (cycle_count >= current_op->done_cycle && current_op->done_cycle > 0) done_after++;
                else pending_after++;
            } else {
                if (cycle_count >= current_op->done_cycle && current_op->done_cycle > 0) done_before++;
                else pending_before++;
            }
        }
        current_op = current_op->next_node;
    }

    // --- [수정] 두 파일에 모두 동일한 내용을 기록하는 로직 ---
    
    // 1. recovery_log (전용 파일)에 기록
    if (recovery_log_file) {
        fprintf(recovery_log_file, "[Mispred for PC 0x%llx] [Cycle %-10llu] op_num:%-10llu (off_path:%d) Mispred_Type:%-4s Conf:%d (Counter:%u) Target:0x%llx\n",
                pc_val, cycle_count, op->op_num, op->off_path, op->oracle_info.mispred ? "MISP" : (op->oracle_info.misfetch ? "MISF" : "----"), op->oracle_info.pred_conf, local_bpc_data_ptr->bpc_ctr_table[op->oracle_info.pred_conf_index], op->oracle_info.npc);
        fprintf(recovery_log_file, "  Next Fetch Addr: 0x%llx, Recovery Ends at Cycle: %-10llu\n",
                bp_recovery_info->recovery_fetch_addr, bp_recovery_info->recovery_cycle);
        fprintf(recovery_log_file, "  ROB state | Before op: Done=%-3d, Pending=%-3d | After op: Done=%-3d, Pending=%-3d | Miss (Off:%-3d, On:%-3d) | Total=%-3d\n",
                done_before, pending_before, done_after, pending_after, miss_off_path_count, miss_on_path_count, node->node_count);
        fprintf(recovery_log_file, "  RS non ready state  | Off-path ops: %-3d, On_path ops: %-3d\n",
                rs_off_path_count, rs_on_path_count);
        fflush(recovery_log_file);
    }

    // 2. events_log (공유 파일)에 기록
    // retired_op_log_file 포인터는 op_trace_log.h를 통해 접근합니다.
    if (retired_op_log_file) {
        fprintf(retired_op_log_file, "[Mispred for PC 0x%llx] [Cycle %-10llu] op_num:%-10llu (off_path:%d) Mispred_Type:%-4s Conf:%d (Counter:%u) Target:0x%llx\n",
                pc_val, cycle_count, op->op_num, op->off_path, op->oracle_info.mispred ? "MISP" : (op->oracle_info.misfetch ? "MISF" : "----"), op->oracle_info.pred_conf, local_bpc_data_ptr->bpc_ctr_table[op->oracle_info.pred_conf_index], op->oracle_info.npc);
        fprintf(retired_op_log_file, "  Next Fetch Addr: 0x%llx, Recovery Ends at Cycle: %-10llu\n",
                bp_recovery_info->recovery_fetch_addr, bp_recovery_info->recovery_cycle);
        fprintf(retired_op_log_file, "  ROB state | Before op: Done=%-3d, Pending=%-3d | After op: Done=%-3d, Pending=%-3d | Miss (Off:%-3d, On:%-3d) | Total=%-3d\n",
                done_before, pending_before, done_after, pending_after, miss_off_path_count, miss_on_path_count, node->node_count);
        fprintf(retired_op_log_file, "  RS non ready state  | Off-path ops: %-3d, On_path ops: %-3d\n",
                rs_off_path_count, rs_on_path_count);
        fflush(retired_op_log_file);
    }
}

void log_recovery_end(Node_Stage* node, Counter cycle_count, Bp_Recovery_Info* bp_recovery_info) {
    if ((!recovery_log_file && !retired_op_log_file) || cycle_count < DEBUG_CYCLE_START || cycle_count > DEBUG_CYCLE_STOP) return;

    Op* op = bp_recovery_info->recovery_op;
    Addr pc_val = op->inst_info->addr;

    int done_before = 0, pending_before = 0, done_after = 0, pending_after = 0;
    int rs_off_path_count = 0, rs_on_path_count = 0;
    int miss_off_path_count = 0, miss_on_path_count = 0;
    Flag op_found = FALSE;

    Op* current_op = node->node_head;
    while(current_op) {
        if (current_op->state == OS_IN_RS) {
            if (current_op->off_path) rs_off_path_count++;
            else rs_on_path_count++;
        } else if (current_op->state == OS_MISS) {
            if (current_op->off_path) miss_off_path_count++;
            else miss_on_path_count++;
        }

        if (current_op == op) {
            op_found = TRUE;
        } else {
            if (op_found) {
                if (cycle_count >= current_op->done_cycle && current_op->done_cycle > 0) done_after++;
                else pending_after++;
            } else {
                if (cycle_count >= current_op->done_cycle && current_op->done_cycle > 0) done_before++;
                else pending_before++;
            }
        }
        current_op = current_op->next_node;
    }

    // --- [수정] 두 파일에 모두 동일한 내용을 기록하는 로직 ---
    
    // 1. recovery_log (전용 파일)에 기록
    if (recovery_log_file) {
        fprintf(recovery_log_file, "[Recovery End for PC 0x%llx] [Cycle %-10llu] op_num:%-10llu (off_path:%d) Mispred_Type:%-4s Conf:%d (Counter:%u) Target:0x%llx\n",
                pc_val, cycle_count, op->op_num, op->off_path, op->oracle_info.mispred ? "MISP" : (op->oracle_info.misfetch ? "MISF" : "----"), op->oracle_info.pred_conf, local_bpc_data_ptr->bpc_ctr_table[op->oracle_info.pred_conf_index], op->oracle_info.npc);
        fprintf(recovery_log_file, "  Next Fetch Addr: 0x%llx\n",
                bp_recovery_info->recovery_fetch_addr);
        fprintf(recovery_log_file, "  ROB state | Before op: Done=%-3d, Pending=%-3d | After op: Done=%-3d, Pending=%-3d | Miss (Off:%-3d, On:%-3d) | Total=%-3d\n",
                done_before, pending_before, done_after, pending_after, miss_off_path_count, miss_on_path_count, node->node_count);
        fprintf(recovery_log_file, "  RS non ready state  | Off-path ops: %-3d, On_path ops: %-3d\n",
                rs_off_path_count, rs_on_path_count);
        fflush(recovery_log_file);
    } 
    
    // 2. events_log (공유 파일)에 기록
    if (retired_op_log_file) {
        fprintf(recovery_log_file, "[Recovery End for PC 0x%llx] [Cycle %-10llu] op_num:%-10llu (off_path:%d) Mispred_Type:%-4s Conf:%d (Counter:%u) Target:0x%llx\n",
                pc_val, cycle_count, op->op_num, op->off_path, op->oracle_info.mispred ? "MISP" : (op->oracle_info.misfetch ? "MISF" : "----"), op->oracle_info.pred_conf, local_bpc_data_ptr->bpc_ctr_table[op->oracle_info.pred_conf_index], op->oracle_info.npc);
        fprintf(retired_op_log_file, "  Next Fetch Addr: 0x%llx\n",
                bp_recovery_info->recovery_fetch_addr);
        fprintf(retired_op_log_file, "  ROB state | Before op: Done=%-3d, Pending=%-3d | After op: Done=%-3d, Pending=%-3d | Miss (Off:%-3d, On:%-3d) | Total=%-3d\n",
                done_before, pending_before, done_after, pending_after, miss_off_path_count, miss_on_path_count, node->node_count);
        fprintf(retired_op_log_file, "  RS non ready state  | Off-path ops: %-3d, On_path ops: %-3d\n",
                rs_off_path_count, rs_on_path_count);
        fflush(retired_op_log_file);
    }
}
