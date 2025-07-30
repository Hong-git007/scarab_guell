// fill_buffer_log.h

#ifndef __FILL_BUFFER_LOG_H__
#define __FILL_BUFFER_LOG_H__

#include "../fill_buffer.h"

void init_fill_buffer_log(void);
void finalize_fill_buffer_log(void);
void log_fill_buffer_entry(uns proc_id, Fill_Buffer* fb, Counter cycle_count);

#endif