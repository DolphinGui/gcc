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
  void* lp;
  unsigned lp_arg;
  unsigned _reserved;
} personality_result;

typedef struct {
  fae_data_entry *result;
  personality_result r;
} find_ptr_result;
/* The C++-like personality. Prohibited from using values over 2 ^ 16, although
 * I doubt you'll have that many types. */
personality_result __fae_personality_v1(void *lsda, void *exception);
void __fae_get_ptr(find_ptr_result* out, struct _Unwind_Exception *except, const char *pc);
#endif
