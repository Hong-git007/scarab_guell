/***************************************************************************************
 * File         : on_off_path_cache.h
 * Author       : Gaudi Lab
 * Date         : 3/8/1999, 4/15/2025
 * Description  :
 ***************************************************************************************/

#ifndef __ON_OFF_PATH_CACHE_H__
#define __ON_OFF_PATH_CACHE_H__

#include "globals/global_types.h"
#include "op.h"
#include "table_info.h"

// 캐시와 경로의 최대 크기를 정의합니다.
#define ON_OFF_PATH_CACHE_SIZE 1024
// 최대 경로 길이는 Fill Buffer 크기와 동일하게 설정하는 것이 일반적입니다.
#define MAX_ON_OFF_PATH_LENGTH 256 

// On/Off 경로 캐시의 한 엔트리
typedef struct On_Off_Path_Cache_Entry_struct {
    Flag         is_valid;
    Addr         h2p_branch_pc;       // 캐시를 식별할 H2P 브랜치의 PC
    Counter      h2p_branch_op_num;   // H2P 브랜치의 op_num
    uns          path_length;         // 저장된 경로의 길이
    Op           path[MAX_ON_OFF_PATH_LENGTH]; // 실제 경로 데이터
} On_Off_Path_Cache_Entry;


// 함수 프로토타입 선언
void init_on_off_path_cache(uns proc_id);
void reset_on_off_path_cache(uns proc_id);
void record_on_off_path(uns proc_id, Op* h2p_op_at_head);

#endif // __ON_OFF_PATH_CACHE_H__
