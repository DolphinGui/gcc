#include "auto-target.h"
#include "tconfig.h"
#include "tsystem.h"

#include "unwind-generic.h"
#include "unwind-fae-sup.h"

#define fassert(expr)                                                          \
  if (!(expr))                                                                    \
    __builtin_trap()

extern const fae_table_entry __fae_table_start[];
extern const fae_table_entry __fae_table_stop[];

// returns data pointer for pc entry. If no entry is found, return 0
void  __fae_get_ptr(find_ptr_result* out,struct _Unwind_Exception *except,
                              const char *pc) {
  for (const fae_table_entry *it = __fae_table_start; it < __fae_table_stop; ++it) {
    if (it->begin < pc && pc <= it->end) {
      // no lsda, just unwind
      fae_data_entry *data = it->data;
      if (!data->lsda) {
        out->result = data;
        
        out->lp = 0;
        out->lp_arg = 0xffffffff;
        return;
      }
      // For now we only support c++ personalities. Maybe hypothetically
      // different language runtimes will use different personalities, but
      // for now we only support c++.
      personality_fn* personality = (personality_fn*) data->lsda->personality;
      out->result = data;
      personality_result r;
      r = personality(pc - it->begin, data->lsda, except);
      out->lp = it->begin + r.lp;
      out->lp_arg = r.lp_arg;
      out->_reserved = r._reserved;
      return;
    }
  }
  __builtin_trap();
}
