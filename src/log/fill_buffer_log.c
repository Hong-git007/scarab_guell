// fill_buffer_log.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fill_buffer_log.h"
#include "../globals/utils.h"
#include "../isa/isa.h"
#include "../table_info.h"
#include "debug/debug.param.h"
#include "../debug/debug_print.h"

extern char* OUTPUT_DIR;

static FILE* fill_buffer_log_file = NULL;

#define REG(x) #x,
static const char* reg_names[NUM_REGS] = {
#include "../isa/x86_regs.def"
};
#undef REG

#define REG_ARCH 0

static void close_fill_buffer_log(void) {
    if (fill_buffer_log_file) fclose(fill_buffer_log_file);
}

void init_fill_buffer_log(void) {
    if (fill_buffer_log_file == NULL) {
        fill_buffer_log_file = file_tag_fopen(OUTPUT_DIR, "fill_buffer", "w");
        if (fill_buffer_log_file) atexit(close_fill_buffer_log);
    }
}

void finalize_fill_buffer_log(void) {
    close_fill_buffer_log();
}

static char* disasm_retired_op(Retired_Op_Info* op) {
    static char buf[512];
    int i = 0;

    // opcode 문자열은 op->cf_type 등으로 적절히 결정 필요
    // 예시는 cf_type 존재 시 해당 이름, 없으면 "OP"로 표시
    const char* opcode = Op_Type_str(op->op_type);
    if (op->op_type == OP_CF) {
        opcode = cf_type_names[op->cf_type];
    } else {
        opcode = Op_Type_str(op->op_type);
    }

    i += sprintf(&buf[i], "%-8s ", opcode);

    // dest 레지스터 출력
    for (int j = 0; j < op->num_dest_regs; j++) {
        int reg_id = op->dst_reg_id[j][REG_ARCH];
        const char* reg_name = (reg_id < NUM_REGS) ? reg_names[reg_id] : "??";
        i += sprintf(&buf[i], "r%d(%s)%s", reg_id, reg_name, (j == op->num_dest_regs - 1) ? "" : ",");
    }
    if (op->num_dest_regs > 0 && op->num_src_regs > 0) {
        i += sprintf(&buf[i], " <- ");
    }

    // src 레지스터 출력
    for (int j = 0; j < op->num_src_regs; j++) {
        int reg_id = op->src_reg_id[j][REG_ARCH];
        const char* reg_name = (reg_id < NUM_REGS) ? reg_names[reg_id] : "??";
        i += sprintf(&buf[i], "r%d(%s)%s", reg_id, reg_name, (j == op->num_src_regs - 1) ? "" : ",");
    }

    if (op->mem_type == MEM_LD && op->mem_size > 0) {
        i += sprintf(&buf[i], " %d@%08x", op->mem_size, (unsigned int)op->va);
    }
    if (op->mem_type == MEM_ST && op->mem_size > 0) {
        i += sprintf(&buf[i], " %d@%08x", op->mem_size, (unsigned int)op->va);
    }

    buf[i] = '\0';
    return buf;
}

void log_fill_buffer_entry(uns proc_id, Fill_Buffer* fb, Counter cycle_count) {
    if (!fill_buffer_log_file || !fb || fb->count == 0 ||
        !(cycle_count >= DEBUG_CYCLE_START && cycle_count <= DEBUG_CYCLE_STOP)) {
        return;
    }

    fprintf(fill_buffer_log_file, "--- [LOG] Fill Buffer for Core %u @ Cycle:%-6llu ---\n", proc_id, cycle_count);
    fprintf(fill_buffer_log_file, "Buffer Size: %u | Count: %u | Head: %u | Tail: %u\n",
            fb->size, fb->count, fb->head, fb->tail);
    fprintf(fill_buffer_log_file, "---------------------------------------------------------\n");

    for (int i = 0; i < fb->count; ++i) {
        int idx = (fb->head + i) % fb->size;
        Retired_Op_Info* op = &fb->entries[idx];
        char* disasm_str = disasm_retired_op(op);

        fprintf(fill_buffer_log_file,
            "[%3d] PC: 0x%08llx | OpNum: %-10llu | H2P: %s | Disasm: %-45s\n",
            i,
            op->pc,
            op->op_num,
            op->hbt_pred_is_hard ? "O" : "X",
            disasm_str);
    }

    fprintf(fill_buffer_log_file, "-------------------------- END LOG ---------------------------\n\n");
    fflush(fill_buffer_log_file);
}
