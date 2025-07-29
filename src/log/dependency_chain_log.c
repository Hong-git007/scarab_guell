#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dependency_chain_log.h"
#include "../globals/utils.h"
#include "../table_info.h" 
#include "../isa/isa.h" // NUM_REGS, Reg_Id 등을 위해 포함
#include "debug/debug.param.h"
#include "../debug/debug_print.h"

extern char* OUTPUT_DIR;
static FILE* dependency_chain_log_file = NULL;

// ----- [ 개선사항 1: 레지스터 이름 배열 추가 ] -----
// disasm_reg와 동일한 방식으로 파일 내에 static 배열을 만듭니다.
#define REG(x) #x,
static const char* reg_names[NUM_REGS] = {
#include "../isa/x86_regs.def" // x86_regs.def 파일 경로에 맞게 수정
};
#undef REG

// 캐시된 Op 정보를 disasm하는 헬퍼 함수
static char* disasm_cached_op(Chain_Op_Info* op) {
    static char buf[512];
    int i = 0;

    // ----- [ 최종 수정 사항 ] -----
    const char* opcode = Op_Type_str(op->op_type);
    if (op->op_type == OP_CF) {
        opcode = cf_type_names[op->cf_type];
    } else {
        opcode = Op_Type_str(op->op_type);
    }

    i += sprintf(&buf[i], "%-8s ", opcode);

    for (int j = 0; j < op->num_dests; j++) {
        i += sprintf(&buf[i], "r%u(%s)%s", op->dests[j].id, reg_names[op->dests[j].id], (j == op->num_dests - 1) ? "" : ",");
    }
    if (op->num_dests > 0 && op->num_srcs > 0) {
        i += sprintf(&buf[i], " <- ");
    }
    for (int j = 0; j < op->num_srcs; j++) {
        i += sprintf(&buf[i], "r%u(%s)%s", op->srcs[j].id, reg_names[op->srcs[j].id], (j == op->num_srcs - 1) ? "" : ",");
    }
    
    buf[i] = '\0';
    return buf;
}

static void close_dependency_chain_log(void) {
    if (dependency_chain_log_file) fclose(dependency_chain_log_file);
}

void init_dependency_chain_log(void) {
    if (dependency_chain_log_file == NULL) {
        dependency_chain_log_file = file_tag_fopen(OUTPUT_DIR, "dependency_chain", "w");
        if (dependency_chain_log_file) atexit(close_dependency_chain_log);
    }
}

void finalize_dependency_chain_log(void) {
    close_dependency_chain_log();
}

void log_dependency_chain_entry(uns proc_id, Dependency_Chain_Cache_Entry* entry, Counter cycle_count) {
    if (!dependency_chain_log_file || !entry || !entry->is_valid||
        !(cycle_count >= DEBUG_CYCLE_START && cycle_count <= DEBUG_CYCLE_STOP)) return;

    fprintf(dependency_chain_log_file, "--- [LOG] Dependency Chain for Core %u Cycle:%-4llu---\n", proc_id, cycle_count);
    fprintf(dependency_chain_log_file, "Triggering H2P Branch PC: 0x%llx, OpNum: %llu, Chain Length: %u\n",
            entry->h2p_branch_pc, entry->h2p_branch_op_num, entry->chain_length);
    fprintf(dependency_chain_log_file, "------------------------------------------------------------------\n");

    for (int i = 0; i < entry->chain_length; ++i) {
        Chain_Op_Info* op = &entry->chain[i];
        char* disasm_str = disasm_cached_op(op);
        fprintf(dependency_chain_log_file, "[PC: 0x%08llx] OpNum:%-10llu H2p:%s Disasm: %-45s\n",
            op->pc, 
            op->op_num,
            op->is_h2p ? "O" : "X", 
            disasm_str);
    }
    fprintf(dependency_chain_log_file, "-------------------------- END LOG ---------------------------\n\n");
    fflush(dependency_chain_log_file);
}