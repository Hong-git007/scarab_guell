#ifndef __ON_OFF_PATH_LOG_H__
#define __ON_OFF_PATH_LOG_H__

#include "../on_off_path_cache.h" 

void init_on_off_path_log(void);
void finalize_on_off_path_log(void);
void log_on_off_path_entry(uns proc_id, On_Off_Path_Cache_Entry* entry, Counter cycle_count);

#endif // __ON_OFF_PATH_LOG_H__