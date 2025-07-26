#ifndef __OP_TRACE_LOG_H__
#define __OP_TRACE_LOG_H__

#include "globals/global_types.h"
#include "op.h"
#include <stdio.h>

extern FILE* retired_op_log_file;

/*
 * @brief Initializes the op trace logger.
 */
void init_op_trace_log(void);

/*
 * @brief Logs detailed information about an issued operation.
 *
 * @param op The operation being issued.
 * @param cycle_count The current simulation cycle.
 */
void log_fill_rob_op(Op* op, Counter cycle_count);

/*
 * @brief Logs the number of retired ops for the current cycle.
 *
 * @param cycle_count The current simulation cycle.
 * @param ret_count The number of ops retired in the current cycle.
 */
void log_retired_ops(Counter cycle_count, uns ret_count);

#endif // __OP_TRACE_LOG_H__