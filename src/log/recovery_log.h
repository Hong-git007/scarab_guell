#ifndef __RECOVERY_LOG_H__
#define __RECOVERY_LOG_H__

#include "op.h"
#include "node_stage.h"
#include "bp/bp.h"

/*
 * @brief Initializes the recovery logger. Opens the log file.
 */
void init_recovery_log(void);




void log_rat_state(FILE* log_file, const char* title, struct reg_table_entry* entries, uns size);
/*
 * @brief Logs information at the time a misprediction is detected.
 *
 * @param op The mispredicted operation.
 * @param node The current node stage.
 * @param cycle_count The current simulation cycle.
 * @param bp_recovery_info Information about the branch prediction recovery.
 */
void log_misprediction_detection_at_decode(Op* op, Node_Stage* node, Counter cycle_count, Bp_Recovery_Info* bp_recovery_info);
void log_misprediction_detection_at_exec(Op* op, Node_Stage* node, Counter cycle_count, Bp_Recovery_Info* bp_recovery_info);

/*
 * @brief Logs information when the recovery process is complete.
 *
 * @param node The current node stage.
 * @param cycle_count The current simulation cycle.
 * @param bp_recovery_info Information about the branch prediction recovery.
 */
void log_recovery_end(Node_Stage* node, Counter cycle_count, Bp_Recovery_Info* bp_recovery_info);

#endif // __RECOVERY_LOG_H__