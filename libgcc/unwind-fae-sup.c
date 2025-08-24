#include "auto-target.h"
#include "tconfig.h"
#include "tsystem.h"

#include "unwind-fae-sup.h"
#include "unwind-generic.h"

extern const fae_unsorted_entry __fae_unsorted_start[];
extern const fae_unsorted_entry __fae_unsorted_end[];

extern const fae_table_entry __fae_table_start[];
extern const fae_table_entry __fae_table_end[];

extern const fae_data_entry __fae_data_start[];

extern const char __fae_sorted_function_end[];

__attribute__((nothrow)) static void
linear_search(find_ptr_result *out, struct _Unwind_Exception *except,
              const char *pc);

__attribute__((nothrow)) static void
binary_search(find_ptr_result *out, struct _Unwind_Exception *except,
              const char *pc);

// Executes any lsda's associated with the frame, then outputs the results
__attribute__((nothrow)) static void
execute(find_ptr_result *out, struct _Unwind_Exception *except, const char *pc,
        const fae_data_entry *f, const char *fbegin);

// returns data pointer for pc entry. If no entry is found, return 0
__attribute__((nothrow)) void __fae_get_ptr(find_ptr_result *out,
                                            struct _Unwind_Exception *except,
                                            const char *pc) {
  if (pc < __fae_sorted_function_end) {
    binary_search(out, except, pc - 1);
  } else {
    linear_search(out, except, pc);
  }
}

void binary_search(find_ptr_result *out, struct _Unwind_Exception *except,
                   const char *pc) {
  const fae_table_entry *b = __fae_table_start, *e = __fae_table_end;
  // binary search until there's a few left. At which point it's *probably*
  // better to linear search
  while (e - b > 8) {
    unsigned long dist = e - b;
    const fae_table_entry *middle = b + (dist / 2);
    if (middle->begin < pc) {
      b = middle;
    } else {
      e = middle + 1;
    }
  }
  // search the functions backwards so we don't have to check the +1 case
  for (const fae_table_entry *it = e - 1; it > b - 1; --it) {
    if (it->begin <= pc) {
      long index = it - __fae_table_start;
      const fae_data_entry *data = __fae_data_start + index;
      execute(out, except, pc, data, it->begin);
      return;
    }
  }
  out->result = 0;
  out->lp = 0;
  out->lp_arg = 0;
}

void linear_search(find_ptr_result *out, struct _Unwind_Exception *except,
                   const char *pc) {
  for (const fae_unsorted_entry *it = __fae_unsorted_start;
       it < __fae_unsorted_end; ++it) {
    if (it->begin < pc && pc <= it->end) {
      execute(out, except, pc, it->data, it->begin);
      return;
    }
  }
  out->result = 0;
  out->lp = 0;
  out->lp_arg = 0;
}

void execute(find_ptr_result *out, struct _Unwind_Exception *except,
             const char *pc, const fae_data_entry *data, const char *fbegin) {
  // no lsda, just unwind
  if (!data->lsda) {
    out->result = data;
    out->lp = 0;
    out->lp_arg = 0xffffffff;
    return;
  }
  // For now we only support c++ personalities. Maybe hypothetically
  // different language runtimes will use different personalities, but
  // for now we only support c++.
  personality_fn *personality = (personality_fn *)data->lsda->personality;
  out->result = data;
  personality_result r;
  r = personality(pc - fbegin, data->lsda, except);
  out->lp = fbegin + r.lp;
  out->lp_arg = r.lp_arg;
  out->_reserved = r._reserved;
  return;
}
