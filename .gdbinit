# .gdbinit 파일 내용
# ==========================================================
# 1. Python 스크립트 로딩
# ==========================================================
source gdb_branch_recovery.py
start_log recovery_log.txt
set logging file my_op_output.txt
set logging overwrite on
set logging on
set logging off
set logging overwrite off
# ==========================================================
# 2. 기본 breakpoint
# ==========================================================
break bp.c:156
break cmp_model.c:349
# ==========================================================
# 3. 사용자 정의 define 명령어들
# ==========================================================
# ==========================================================
# Op 정보를 터미널과 파일 양쪽에 모두 출력하는 메인 함수
# ==========================================================
define print_op
  set logging file my_op_output.txt
  set $op = node.node_head
  set $current_op = op
  set logging on
  printf "\n====================[ RECOVERY DETECTION ]====================\n"
  printf "│ Recovery caused by PC: 0x%x | Cycle: %-10d | Op#: %-10d | Off-path: %d\n", \
  $current_op->inst_info->addr, cycle_count, $current_op->op_num, $current_op->off_path
  printf "--------------------------------------------------------------\n"
  printf "│ INFO │ ROB / RS 상태 출력 시작\n"
  printf "--------------------------------------------------------------\n"
  while $op
    if $op == $current_op
      printf "-> "
    else
      printf "   "
    end
    set $found_state = 0
    set $misp_str = ($op->engine_info.mispred ? "MISP" : "    ")
    if (int)$op->state == 0
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "FETCHED", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 1
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "ISSUED", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 2
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "IN_RS", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 3
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "SLEEP", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 4
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "WAIT_FWD", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 5
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "LOW_PRIORITY", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 6
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "READY", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 7
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "TENTATIVE", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 8
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "SCHEDULED", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 9
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "MISS", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 10
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "WAIT_DCACHE", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 11
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "WAIT_MEM", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 12
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "DONE", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if $found_state == 0
      printf "Op #%-9d PC: 0x%x | UNKNOWN STATE (%d)\n", $op->op_num, $op->inst_info->addr, $op->state
    end
    printf "      |-- Cycles [Curr:%-9d] Fet:%-9d Map:%-9d Iss:%-9d Rdy:%-9d Sch:%-9d Exe:%-9d Don:%-9d Ret:%-9d\n", cycle_count, $op->fetch_cycle, $op->map_cycle, $op->issue_cycle, $op->rdy_cycle, $op->sched_cycle, $op->exec_cycle, $op->done_cycle, $op->retire_cycle
    if $op->oracle_info.mispred
      printf "      +-> MISPREDICT DETAILS:\n"
      printf "      |   - Predicted: dir=%d, target=0x%x\n", $op->engine_info.pred, $op->engine_info.pred_npc
      printf "      |   - Actual:    dir=%d, target=0x%x\n", $op->oracle_info.dir, $op->oracle_info.target
    end
    set $i = 0
    if $op->oracle_info.num_srcs > 0
      printf "      #-> DEPENDS ON (Producers):\n"
      while $i < $op->oracle_info.num_srcs
        printf "      |   - Op #%-9d with dep type: ", $op->oracle_info.src_info[$i].op_num
        set $dep_type = (int)$op->oracle_info.src_info[$i].type
        if $dep_type == 0
          printf "REG_DATA_DEP\n"
        else
          if $dep_type == 1
            printf "MEM_ADDR_DEP\n"
          else
            if $dep_type == 2
              printf "MEM_DATA_DEP\n"
            else
              printf "UNKNOWN_DEP\n"
            end
          end
        end
        set $i = $i + 1
      end
    end
    set $wakeup_entry = $op->wake_up_head
    if $wakeup_entry
      printf "      *-> WAKES UP (Consumers):\n"
      while $wakeup_entry
        printf "      |   - Op #%-9d with dep type: ", $wakeup_entry->op->op_num
        set $dep_type = (int)$wakeup_entry->dep_type
        if $dep_type == 0
          printf "REG_DATA_DEP\n"
        else
          if $dep_type == 1
            printf "MEM_ADDR_DEP\n"
          else
            if $dep_type == 2
              printf "MEM_DATA_DEP\n"
            else
              printf "UNKNOWN_DEP\n"
            end
          end
        end
        set $wakeup_entry = $wakeup_entry->next
      end
    end
    set $op = $op->next_node
  end
  set logging off
  end

  define print_op_c
  set logging file my_op_output.txt
  set $op = node.node_head
  set $head_op = node.node_head
  set logging on
  printf "\n====================[ RECOVERY COMPLETE ]====================\n"
  printf "│ Recovery caused by PC: 0x%x | Cycle: %-10d | Op#: %-10d | Off-path: %d\n", \
  $op->inst_info->addr, cycle_count, $op->op_num, $op->off_path
  printf "--------------------------------------------------------------\n"
  printf "│ INFO │ ROB / RS 상태 출력 시작\n"
  printf "--------------------------------------------------------------\n"
  while $op
    if $op == $head_op
      printf "-> "
    else
      printf "   "
    end
    set $found_state = 0
    set $misp_str = ($op->engine_info.mispred ? "MISP" : "    ")
    if (int)$op->state == 0
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "FETCHED", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 1
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "ISSUED", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 2
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "IN_RS", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 3
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "SLEEP", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 4
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "WAIT_FWD", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 5
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "LOW_PRIORITY", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 6
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "READY", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 7
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "TENTATIVE", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 8
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "SCHEDULED", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 9
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "MISS", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 10
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "WAIT_DCACHE", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 11
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "WAIT_MEM", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if (int)$op->state == 12
      printf "Op #%-9d PC: 0x%x | %-12s | %s | %-12s dest: r%-2d srcs: r%-2d, r%-2d\n", $op->op_num, $op->inst_info->addr, "DONE", $misp_str, $op->table_info->name, $op->inst_info->dests[0].reg, $op->inst_info->srcs[0].reg, $op->inst_info->srcs[1].reg
      set $found_state = 1
    end
    if $found_state == 0
      printf "Op #%-9d PC: 0x%x | UNKNOWN STATE (%d)\n", $op->op_num, $op->inst_info->addr, $op->state
    end
    printf "      |-- Cycles [Curr:%-9d] Fet:%-9d Map:%-9d Iss:%-9d Rdy:%-9d Sch:%-9d Exe:%-9d Don:%-9d Ret:%-9d\n", cycle_count, $op->fetch_cycle, $op->map_cycle, $op->issue_cycle, $op->rdy_cycle, $op->sched_cycle, $op->exec_cycle, $op->done_cycle, $op->retire_cycle
    if $op->oracle_info.mispred
      printf "      +-> MISPREDICT DETAILS:\n"
      printf "      |   - Predicted: dir=%d, target=0x%x\n", $op->engine_info.pred, $op->engine_info.pred_npc
      printf "      |   - Actual:    dir=%d, target=0x%x\n", $op->oracle_info.dir, $op->oracle_info.target
    end
    set $i = 0
    if $op->oracle_info.num_srcs > 0
      printf "      #-> DEPENDS ON (Producers):\n"
      while $i < $op->oracle_info.num_srcs
        printf "      |   - Op #%-9d with dep type: ", $op->oracle_info.src_info[$i].op_num
        set $dep_type = (int)$op->oracle_info.src_info[$i].type
        if $dep_type == 0
          printf "REG_DATA_DEP\n"
        else
          if $dep_type == 1
            printf "MEM_ADDR_DEP\n"
          else
            if $dep_type == 2
              printf "MEM_DATA_DEP\n"
            else
              printf "UNKNOWN_DEP\n"
            end
          end
        end
        set $i = $i + 1
      end
    end
    set $wakeup_entry = $op->wake_up_head
    if $wakeup_entry
      printf "      *-> WAKES UP (Consumers):\n"
      while $wakeup_entry
        printf "      |   - Op #%-9d with dep type: ", $wakeup_entry->op->op_num
        set $dep_type = (int)$wakeup_entry->dep_type
        if $dep_type == 0
          printf "REG_DATA_DEP\n"
        else
          if $dep_type == 1
            printf "MEM_ADDR_DEP\n"
          else
            if $dep_type == 2
              printf "MEM_DATA_DEP\n"
            else
              printf "UNKNOWN_DEP\n"
            end
          end
        end
        set $wakeup_entry = $wakeup_entry->next
      end
    end
    set $op = $op->next_node
  end
  set logging off
  end


# ==========================================================
# Ready List의 모든 Op를 출력하는 함수
# ==========================================================
define print_ready_list
  printf "--- Ready List (pointed by rdy_head) ---\n"
  set $op = node->rdy_head
  if !$op
    printf "Ready List is empty.\n"
  end
  while $op
    # 기존에 만드신 print_op 함수와 유사하게 상세 정보 출력
    # 여기서는 간단한 예시만 보여드립니다.
    printf "-> Op #%-9d PC: 0x%x | State: %-12s | Name: %-12s\n", \
           $op->op_num, $op->inst_info->addr, "READY", $op->table_info->name
    
    set $op = $op->next_rdy
  end
  printf "--- End of Ready List ---\n"
end

#288
define print_reg_map
  set $i = 0
  while $i < 144
    set $flag = map_data->map_flags[$i]
    set $index = $i * 2 + $flag
    printf "Reg[%d]\t-> op_num: %-10llu", $i, map_data->reg_map[$index].op_num
    if $flag
      printf " (off-path)\n"
    else
      printf " (on-path)\n"
    end
    set $i = $i + 1
  end
end

document print_reg_map
  Prints the current register mapping status for all 144 architectural registers.
  Usage: print_reg_map
end
