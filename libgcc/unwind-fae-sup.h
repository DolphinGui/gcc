#ifndef UNWIND_SUP_H
#define UNWIND_SUP_H

#include "unwind-generic.h"

typedef unsigned short u16;

typedef struct {
  unsigned long ident;
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
  fae_data_entry *result;
  void *lp_arg;
} find_ptr_result;

void *__fae_personality_v1(void *lsda, void *exception);
find_ptr_result __fae_get_ptr(struct _Unwind_Exception *except, const char *pc);
#endif
