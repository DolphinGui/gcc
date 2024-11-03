#include "fae.h"

extern prog_byte __fae_table_start[];
extern prog_byte __fae_table_stop[];

typedef const table_entry_t __maybe_flash * table_ptr;

void __avr_terminate() __attribute__((noreturn));
// no plans to implement forced unwinding
void _Unwind_ForcedUnwind() { __avr_terminate(); }

// returns data pointer for pc entry. If no entry is found, return 0
table_data __fae_get_ptr(void *except, uint16_t pc) {
  table_data result;
  result.type_index = 0xff;

  for (table_ptr ptr = __fae_table_start; ptr < __fae_table_stop; ptr++) {
    if (ptr->pc_begin < pc && pc <= ptr->pc_end) {
      result.data = ptr->data;
      result.data_end = ptr->data + ptr->length;
      result.fp_register = ptr->frame_reg;
      if (ptr->lsda != 0) {
        personality_out out = {};
        result.type_index =
            ptr->personality(ptr->lsda, (pc - ptr->pc_begin) * 2,
                        except, &out);
        result.landing_pad = out.landing_pad;
        if (result.landing_pad == 0) {
          result.type_index = 0xff;
          return result;
        }
        result.landing_pad += ptr->pc_begin * 2;
        result.landing_pad >>= 1;
      }
      return result;
    }
  }
  result.data = 0;
  return result;
}
