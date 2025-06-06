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
        out->r.lp = 0;
        out->r.lp_arg = 0xffffffff;
        return;
      }
      // For now we only support c++ personalities. Maybe hypothetically
      // different language runtimes will use different personalities, but
      // for now we only support c++.
      fassert(data->lsda->ident == 0x2b2b6331656166);
      out->result = data;
      out->r = __fae_personality_v1(data->lsda, except);
      return;
    }
  }
  __builtin_trap();
}
