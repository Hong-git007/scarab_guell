#ifndef __DEPENDENCY_CHAIN_LOG_H__
#define __DEPENDENCY_CHAIN_LOG_H__

#include "dependency_chain_cache.h"

// Function prototypes
void init_dependency_chain_log(void);
void finalize_dependency_chain_log(void);
void log_dependency_chain_entry(uns proc_id, Dependency_Chain_Cache_Entry* entry);

#endif // __DEPENDENCY_CHAIN_LOG_H__
