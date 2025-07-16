#include <stdio.h>
#include <stdlib.h>
#include "recovery_log.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"
#include "op.h"
#include "log/log_params.h"
#include "op_trace_log.h" // [추가] 공유 로그 파일 포인터를 사용하기 위해 헤더를 포함합니다.

// 이 파일 전용 로그(recovery_log.txt)를 위한 포인터입니다.
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
        recovery_log_file = file_tag_fopen(OUTPUT_DIR, "recovery_log.txt", "w");
        if (recovery_log_file == NULL) {
            perror("Error opening recovery_log.txt in output directory");
        } else {
            atexit(close_recovery_log_file);
        }
    }
}

void log_misprediction(Op* op, Node_Stage* node, Counter cycle_count, Bp_Recovery_Info* bp_recovery_info) {
    // [수정] 두 파일 포인터가 모두 유효하지 않은 경우에만 함수를 조기 종료합니다.
    if ((!recovery_log_file && !retired_op_log_file) || cycle_count < log_start_cycle || cycle_count > log_end_cycle) return;

    Addr pc_val = op->inst_info->addr;
    int done_before = 0, pending_before = 0, done_after = 0, pending_after = 0;
    int rs_off_path_count = 0, rs_on_path_count = 0;
    Flag op_found = FALSE;

    Op* current_op = node->node_head;
    while(current_op) {
        if (current_op->state == OS_IN_RS) {
            if (current_op->off_path) rs_off_path_count++;
            else rs_on_path_count++;
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
    
    // 1. recovery_log.txt (전용 파일)에 기록
    if (recovery_log_file) {
        fprintf(recovery_log_file, "[Mispred for PC 0x%llx] [Cycle %-10llu] op_num:%-10llu (off_path:%d) Conf:%d Target:0x%llx\n",
                pc_val, cycle_count, op->op_num, op->off_path, op->oracle_info.pred_conf, op->oracle_info.target);
        fprintf(recovery_log_file, "  Next Fetch Addr: 0x%llx, Recovery Ends at Cycle: %-10llu\n",
                bp_recovery_info->recovery_fetch_addr, bp_recovery_info->recovery_cycle);
        fprintf(recovery_log_file, "  ROB state | Before op: Done=%-3d, Pending=%-3d | After op: Done=%-3d, Pending=%-3d | Total=%-3d\n",
                done_before, pending_before, done_after, pending_after, node->node_count);
        fprintf(recovery_log_file, "  RS non ready state  | Off-path ops: %-3d, On_path ops: %-3d, Total ops: %-3d\n",
                rs_off_path_count, rs_on_path_count, node->rs->rs_op_count);
        fflush(recovery_log_file);
    }

    // 2. events_log.txt (공유 파일)에 기록
    // retired_op_log_file 포인터는 op_trace_log.h를 통해 접근합니다.
    if (retired_op_log_file) {
        fprintf(retired_op_log_file, "[Mispred for PC 0x%llx] [Cycle %-10llu] op_num:%-10llu (off_path:%d) Conf:%d Target:0x%llx\n",
                pc_val, cycle_count, op->op_num, op->off_path, op->oracle_info.pred_conf, op->oracle_info.target);
        fprintf(retired_op_log_file, "  Next Fetch Addr: 0x%llx, Recovery Ends at Cycle: %-10llu\n",
                bp_recovery_info->recovery_fetch_addr, bp_recovery_info->recovery_cycle);
        fprintf(retired_op_log_file, "  ROB state | Before op: Done=%-3d, Pending=%-3d | After op: Done=%-3d, Pending=%-3d | Total=%-3d\n",
                done_before, pending_before, done_after, pending_after, node->node_count);
        fprintf(retired_op_log_file, "  RS non ready state  | Off-path ops: %-3d, On_path ops: %-3d, Total ops: %-3d\n",
                rs_off_path_count, rs_on_path_count, node->rs->rs_op_count);
        fflush(retired_op_log_file);
    }
}

void log_recovery_end(Node_Stage* node, Counter cycle_count, Bp_Recovery_Info* bp_recovery_info) {
    // [수정] 두 파일 포인터가 모두 유효하지 않은 경우에만 함수를 조기 종료합니다.
    if ((!recovery_log_file && !retired_op_log_file) || cycle_count < log_start_cycle || cycle_count > log_end_cycle) return;

    Op* op = bp_recovery_info->recovery_op;
    Addr pc_val = op->inst_info->addr;

    int done_before = 0, pending_before = 0, done_after = 0, pending_after = 0;
    int rs_off_path_count = 0, rs_on_path_count = 0;
    Flag op_found = FALSE;

    Op* current_op = node->node_head;
    while(current_op) {
        if (current_op->state == OS_IN_RS) {
            if (current_op->off_path) rs_off_path_count++;
            else rs_on_path_count++;
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
    
    // 1. recovery_log.txt (전용 파일)에 기록
    if (recovery_log_file) {
        fprintf(recovery_log_file, "[Recovery End for PC 0x%llx] [Cycle %-10llu] op_num:%-10llu (off_path:%d) Conf:%d Target:0x%llx\n",
                pc_val, cycle_count, op->op_num, op->off_path, op->oracle_info.pred_conf, op->oracle_info.target);
        fprintf(recovery_log_file, "  Next Fetch Addr: 0x%llx\n",
                bp_recovery_info->recovery_fetch_addr);
        fprintf(recovery_log_file, "  ROB state | Before op: Done=%-3d, Pending=%-3d | After op: Done=%-3d, Pending=%-3d | Total=%-3d\n",
                done_before, pending_before, done_after, pending_after, node->node_count);
        fprintf(recovery_log_file, "  RS non ready state  | Off-path ops: %-3d, On_path ops: %-3d, Total ops: %-3d\n",
                rs_off_path_count, rs_on_path_count, node->node_count);
        fflush(recovery_log_file);
    }
    
    // 2. events_log.txt (공유 파일)에 기록
    if (retired_op_log_file) {
        fprintf(retired_op_log_file, "[Recovery End for PC 0x%llx] [Cycle %-10llu] op_num:%-10llu (off_path:%d) Conf:%d Target:0x%llx\n",
                pc_val, cycle_count, op->op_num, op->off_path, op->oracle_info.pred_conf, op->oracle_info.target);
        fprintf(retired_op_log_file, "  Next Fetch Addr: 0x%llx\n",
                bp_recovery_info->recovery_fetch_addr);
        fprintf(retired_op_log_file, "  ROB state | Before op: Done=%-3d, Pending=%-3d | After op: Done=%-3d, Pending=%-3d | Total=%-3d\n",
                done_before, pending_before, done_after, pending_after, node->node_count);
        fprintf(retired_op_log_file, "  RS non ready state  | Off-path ops: %-3d, On_path ops: %-3d, Total ops: %-3d\n",
                rs_off_path_count, rs_on_path_count, node->node_count);
        fflush(retired_op_log_file);
    }
}