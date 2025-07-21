#include <stdio.h>
#include <stdlib.h>
#include "op_trace_log.h"
#include "globals/global_vars.h"
#include "globals/utils.h"
#include "op.h"
#include "debug/debug_print.h"
#include "debug/debug.param.h"
#include "bp/bp_conf.h"

static FILE* op_asm_log_file = NULL;
FILE* retired_op_log_file = NULL;
extern char* OUTPUT_DIR;

static void close_op_asm_log_file(void) {
    if (op_asm_log_file) {
        fclose(op_asm_log_file);
        op_asm_log_file = NULL;
    }
}

static void close_retired_op_log_file(void) {
    if (retired_op_log_file) {
        fclose(retired_op_log_file);
        retired_op_log_file = NULL;
    }
}

void init_op_trace_log(void) {
    if (op_asm_log_file == NULL) {
        op_asm_log_file = file_tag_fopen(OUTPUT_DIR, "op_log_full_asm", "w");
        if (op_asm_log_file == NULL) {
            perror("Error opening op_log_full_asm in output directory");
        } else {
            atexit(close_op_asm_log_file);
        }
    }
    if (retired_op_log_file == NULL) {
        retired_op_log_file = file_tag_fopen(OUTPUT_DIR, "retired_op_per_cycle", "w");
        if (retired_op_log_file == NULL) {
            perror("Error opening retired_op_per_cycle in output directory");
        } else {
            atexit(close_retired_op_log_file);
        }
    }

    if (local_bpc_data_ptr == NULL) {
        local_bpc_data_ptr = get_bpc_data();
    }
}

void log_issued_op(Op* op, Counter cycle_count) {
    if (op_asm_log_file && cycle_count >= DEBUG_CYCLE_START && cycle_count <= DEBUG_CYCLE_STOP) {
        const char* disasm_str = disasm_op(op, TRUE);
        char func_op_str[MAX_STR_LENGTH * 4];
        sprint_func_op(func_op_str, op);
 
        if (op->table_info->cf_type) {
            fprintf(op_asm_log_file, "Cycle:%-10llu OpNum:%-10llu PC:0x%-10llx OffPath:%d Mispred_Type:%-4s Disasm: %-40s Conf:%d (Counter:%u) Target:0x%-10llx Dir:%d PredNPC:0x%llx Pred:%d\n",
                    cycle_count,
                    op->op_num,
                    op->inst_info->addr,
                    op->off_path,
                    op->oracle_info.mispred ? "MISP" : (op->oracle_info.misfetch ? "MISF" : "----"),
                    disasm_str,
                    op->oracle_info.pred_conf,
                    local_bpc_data_ptr->bpc_ctr_table[op->oracle_info.pred_conf_index],
                    op->oracle_info.npc,
                    op->oracle_info.dir,
                    op->oracle_info.pred_npc,
                    op->oracle_info.pred);
        } else {
            fprintf(op_asm_log_file, "Cycle:%-10llu OpNum:%-10llu PC:0x%-10llx OffPath:%d Disasm: %-40s\n",
                    cycle_count,
                    op->op_num,
                    op->inst_info->addr,
                    op->off_path,
                    disasm_str);
        }
        fflush(op_asm_log_file);
    }
}

void log_retired_ops(Counter cycle_count, uns ret_count) {
    if (retired_op_log_file && cycle_count >= DEBUG_CYCLE_START && cycle_count <= DEBUG_CYCLE_STOP) {
        if (ret_count > 0) {
            fprintf(retired_op_log_file, "Cycle:%-10llu Retired Ops: %u\n", cycle_count, ret_count);
            fflush(retired_op_log_file);
        }
    }
}
