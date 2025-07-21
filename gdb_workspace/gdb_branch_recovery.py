import gdb

# 전역 변수
mispred_counts = {}
log_file = None

class StartLogCommand(gdb.Command):
    """로깅을 시작하는 사용자 정의 명령어.
    사용법: start_log <파일명>
    """
    def __init__(self):
        super(StartLogCommand, self).__init__("start_log", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        global log_file
        if not arg:
            print("Usage: start_log <filename>")
            return
        
        if log_file:
            log_file.close()
        
        try:
            log_file = open(arg, 'w')
            print("Logging to '%s'" % arg)
        except Exception as e:
            print("Error opening file: %s" % e)


class StopLogCommand(gdb.Command):
    """로깅을 중지하는 사용자 정의 명령어.
    사용법: stop_log
    """
    def __init__(self):
        super(StopLogCommand, self).__init__("stop_log", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        global log_file
        if log_file:
            log_file.close()
            log_file = None
            print("Stopped logging.")
        else:
            print("Logging is not active.")


class LogRecoveryEnd(gdb.Command):
    """복구 완료 시점의 정보를 기록하는 사용자 정의 명령어.
    사용법: log_recovery_end
    """
    def __init__(self):
        super(LogRecoveryEnd, self).__init__("log_recovery_end", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        global log_file
        global mispred_counts
        
        # GDB 컨텍스트에서 변수들을 가져옴
        try:
            # 복구 정보를 담고 있는 구조체에 접근
            bp_recovery_info = gdb.parse_and_eval('bp_recovery_info')
            # 복구를 유발한 op
            op = bp_recovery_info['recovery_op']
            # 현재 node stage와 cycle_count
            node = gdb.parse_and_eval('node')
            cycle_count = gdb.parse_and_eval('cycle_count')
        except gdb.error as e:
            gdb.write("Error accessing GDB variables: %s\n" % e)
            return

        # 필요한 값들을 Python 변수로 변환
        pc_val = int(op['inst_info']['addr'])
        op_num_val = int(op['op_num'])
        off_path_val = int(op['off_path'])
        
        # 복구가 끝난 시점의 사이클
        recovery_end_cycle_val = int(cycle_count)
        next_fetch_addr_val = int(bp_recovery_info['recovery_fetch_addr'])
        
        total_rob_val = int(node['node_count'])
        total_rs_val = int(node['rs']['rs_op_count'])
        
        # ROB/RS 상태 계산
        op_found = False
        done_before, pending_before, done_after, pending_after, rs_off_path_count, rs_on_path_count = 0, 0, 0, 0, 0, 0
        current_op = node['node_head']
        
        while current_op:
            # OS_IN_RS의 enum 값이 2라고 가정 (실제 값에 따라 수정 필요)
            if int(current_op['state']) == 2 and int(current_op['off_path']):
                rs_off_path_count += 1
            if int(current_op['state']) == 2 and int(current_op['off_path']) == 0:
                rs_on_path_count += 1
            
            if current_op == op:
                op_found = True
            else:
                if op_found:
                    if int(cycle_count) >= int(current_op['done_cycle']) >= 1:
                        done_after += 1
                    else:
                        pending_after += 1
                else:
                    if int(cycle_count) >= int(current_op['done_cycle'] >= 1):
                        done_before += 1
                    else:
                        pending_before += 1
            current_op = current_op['next_node']
        
        # PC별 예측 실패 횟수 계산
        if pc_val not in mispred_counts:
            mispred_counts[pc_val] = 0
        pc_mispred_count = mispred_counts[pc_val]

        # 로그 메시지 생성
        log1 = "[Recovery End for PC 0x%x #%-4d] [Cycle %-10d] op_num:%-10d (off_path:%d)\n" % (
            pc_val, pc_mispred_count, recovery_end_cycle_val, op_num_val, off_path_val)
        log2 = "  Next Fetch Addr: 0x%x\n" % (
            next_fetch_addr_val)
        log3 = "  ROB state | Before op: Done=%-3d, Pending=%-3d | After op: Done=%-3d, Pending=%-3d | Total=%-3d\n" % (
            done_before, pending_before, done_after, pending_after, total_rob_val)
        log4 = "  RS non ready state  | Off-path ops: %-3d, On_path ops: %-3d, Total ops: %-3d\n" % (
            rs_off_path_count, rs_on_path_count, total_rs_val)
        full_log = log1 + log2 + log3 + log4

        if log_file:
            log_file.write(full_log)
            log_file.flush()

        gdb.write(full_log)

class MispredLogger(gdb.Command):
    """예측 실패 정보를 기록하는 사용자 정의 명령어.
    사용법: log_mispred
    """
    def __init__(self):
        super(MispredLogger, self).__init__("log_mispred", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        global log_file
        global mispred_counts
        
        # GDB 컨텍스트에서 변수들을 가져옴
        try:
            op = gdb.parse_and_eval('op')
            node = gdb.parse_and_eval('node')
            cycle_count = gdb.parse_and_eval('cycle_count')
            bp_recovery_info = gdb.parse_and_eval('bp_recovery_info')
        except gdb.error as e:
            gdb.write("Error accessing GDB variables: %s\n" % e)
            return

        # 필요한 값들을 Python 변수로 변환
        pc_val = int(op['inst_info']['addr'])
        op_num_val = int(op['op_num'])
        off_path_val = int(op['off_path'])
        recovery_cycle_val = int(bp_recovery_info['recovery_cycle'])
        next_fetch_addr_val = int(bp_recovery_info['recovery_fetch_addr'])
        total_rob_val = int(node['node_count'])
        total_rs_val = int(node['rs']['rs_op_count'])
        
        # ROB/RS 상태 계산
        op_found = False
        done_before, pending_before, done_after, pending_after, rs_off_path_count, rs_on_path_count = 0, 0, 0, 0, 0, 0
        current_op = node['node_head']
        
        while current_op:
            # OS_IN_RS의 enum 값이 2라고 가정 (실제 값에 따라 수정 필요)
            
            '''define OP_STATE_LIST(elem)                                                    \
                elem(FETCHED)  /* op has been fetched, awaiting issue */                     \
                elem(ISSUED) /* op has been issued into the node table (reorder buffer) */ \
                elem(IN_RS)  /* op is in the scheduling window (RS), waiting for its       \
                                sources */                                                 \
                elem(SLEEP)  /* for pipelined schedule: wake up NEXT cycle */              \
                elem(WAIT_FWD)     /* op is waiting for forwarding to happen */            \
                elem(LOW_PRIORITY) /* op is waiting for forwarding to happen */            \
                elem(READY)        /* op is ready to fire, awaiting scheduling */          \
                elem(TENTATIVE)    /* op has been scheduled, but may fail and have to be   \
                                    rescheduled */                                       \
                elem(SCHEDULED)    /* op has been scheduled and will complete */           \
                elem(MISS)         /* op has missed in the dcache */                       \
                elem(WAIT_DCACHE)  /* op is waiting for a dcache port */                   \
                elem(WAIT_MEM)     /* op is waiting for a miss_buffer entry */             \
                elem(DONE)         /* op is finished executing, awaiting retirement */
            '''
            
            if int(current_op['state']) == 2 and int(current_op['off_path']):
                rs_off_path_count += 1
            if int(current_op['state']) == 2 and int(current_op['off_path']) == 0:
                rs_on_path_count += 1

            if current_op == op:
                op_found = True
            else:
                if op_found:
                    if int(cycle_count) >= int(current_op['done_cycle']) >= 1:
                        done_after += 1
                    else:
                        pending_after += 1
                else:
                    if int(cycle_count) >= int(current_op['done_cycle']) >= 1:
                        done_before += 1
                    else:
                        pending_before += 1
            current_op = current_op['next_node']
        
        # PC별 예측 실패 횟수 계산
        if pc_val not in mispred_counts:
            mispred_counts[pc_val] = 0
        mispred_counts[pc_val] += 1
        pc_mispred_count = mispred_counts[pc_val]

        # 로그 메시지 생성
        log1 = "[Mispred for PC 0x%x #%-4d] [Cycle %-10d] op_num:%-10d (off_path:%d)\n" % (
            pc_val, pc_mispred_count, int(cycle_count), op_num_val, off_path_val)
        log2 = "  Next Fetch Addr: 0x%x, Recovery Ends at Cycle: %-10d\n" % (
            next_fetch_addr_val, recovery_cycle_val)
        log3 = "  ROB state | Before op: Done=%-3d, Pending=%-3d | After op: Done=%-3d, Pending=%-3d | Total=%-3d\n" % (
            done_before, pending_before, done_after, pending_after, total_rob_val)
        log4 = "  RS non ready state  | Off-path ops: %-3d, On_path ops: %-3d, Total ops: %-3d\n" % (
            rs_off_path_count, rs_on_path_count, total_rs_val)
        full_log = log1 + log2 + log3 + log4

        if log_file:
            log_file.write(full_log)
            log_file.flush()

        gdb.write(full_log)

# GDB에 새로운 명령어들 등록
StartLogCommand()
StopLogCommand()
LogRecoveryEnd()
MispredLogger()

gdb.write("Custom commands 'start_log', 'stop_log', 'log_recovery_end', 'log_mispred' are defined.\n")
gdb.write("To use, source this file in GDB: (gdb) source gdb_branch_recovery.py\n")
