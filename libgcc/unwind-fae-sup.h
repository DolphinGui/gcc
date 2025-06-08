#ifndef UNWIND_SUP_H
#define UNWIND_SUP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "unwind-generic.h"

typedef unsigned short u16;

typedef struct {
  void* personality;
}lsda_head;

typedef struct {
  unsigned long stack;
  const char *unwinder;
  lsda_head *lsda;
} fae_data_entry;

typedef struct {
  const char *begin;
  const char *end;
  fae_data_entry *data;
} fae_table_entry;

typedef struct {
  unsigned long lp; // the personality only actually returns an offset.
  unsigned lp_arg;
  unsigned _reserved;
} personality_result;

typedef struct {
  fae_data_entry *result;
  const char* lp;
  unsigned lp_arg;
  unsigned _reserved;
} find_ptr_result;

/* The personality function signature. The first thing in the LSDA section. Shouldn't be null. */ 
typedef personality_result (personality_fn)(unsigned long pc_offset, void *lsda, void *exception);

void __fae_get_ptr(find_ptr_result* out, struct _Unwind_Exception *except, const char *pc);

#ifdef __cplusplus
}
#endif


#endif
