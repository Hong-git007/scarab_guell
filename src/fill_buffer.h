#ifndef __FILL_BUFFER_H__
#define __FILL_BUFFER_H__

#include "globals/global_types.h"
#include "op.h" // For Op struct definition
#include "core.param.h"
#include "table_info.h"

typedef struct Fill_Buffer_struct {
  Op*             entries;
  int             head;
  int             tail;
  int             count;
  int             size;
  char*           name;
} Fill_Buffer;

extern Fill_Buffer** retired_fill_buffers;

// Function prototypes
void init_fill_buffer(uns proc_id, const char* name);
void fill_buffer_add(uns proc_id, Op* op);
void reset_fill_buffer(uns proc_id);

#endif // __FILL_BUFFER_H__
