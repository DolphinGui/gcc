#ifdef __AVR_ARCH__



#include <bits/c++config.h>
#include <bits/exception_defines.h>
#include <cxxabi.h>
#include "unwind-cxx.h"
#include <cstdlib>

#include <typeinfo>
#include "unwind-pe.h"
#include <avr/pgmspace.h>

typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
typedef int int16_t;

namespace __cxxabiv1{
static void *get_adjusted_ptr(const void *c, const std::type_info *t,
                              void *thrown_ptr) noexcept {
  auto catch_type = static_cast<const std::type_info *>(c);
  auto throw_type = static_cast<const std::type_info *>(t);
  if (throw_type->__is_pointer_p())
    thrown_ptr = *(void **)thrown_ptr;

  if (catch_type->__do_catch(throw_type, &thrown_ptr, 1)) {
    return thrown_ptr;
  }

  return nullptr;
}

static void *get_adjusted_ptr(void *exc,
                                        const void *catch_type) noexcept {
  auto ue = static_cast<_Unwind_Exception *>(exc);
  auto cxa_except = __get_exception_header_from_ue(ue);
  auto thrown_obj = __get_object_from_ue(ue);
  auto thrown_type = cxa_except->exceptionType;
  cxa_except->adjustedPtr =
      get_adjusted_ptr(catch_type, thrown_type, thrown_obj);
  return cxa_except->adjustedPtr;
}

extern "C" void __avr_terminate() {
  std::terminate();
}

typedef const uint8_t prog_byte;
typedef void* void_ptr;
typedef const void_ptr *prog_ptr;


static uint8_t get(prog_byte* p){
  return __LPM((unsigned)p);
}

static void* getw(prog_ptr p){
  return (void*) __LPM_word((unsigned)p);
}

static uint8_t uleb(prog_byte *ptr, uint16_t *out) {
  *out = get(ptr++);
  if (!(*out & 0b10000000)) {
    return 1;
  } else {
    *out &= 0b01111111;
    if (get(ptr) & 0b00000001) {
      *out |= 0b10000000;
    }
    *out |= get(ptr) << 7;
    return 2;
  }
}


static uint8_t sleb(prog_byte *ptr, int16_t *const val) {
  int16_t i = 0;

  uint8_t b = get(&ptr[i++]);
  *val = b & 0b01111111;

  if (b & 0b10000000) {
    b = get(&ptr[i++]);
    *val <<= 7;
    *val |= b & 0b01111111;
  }

  if (b & 0x40) {
    *val |= (-1ULL) << 7;
    return i;
  }
  return i;
}

struct personality_out{
  uint16_t landing_pad;
};


// this is very bad, and probably should be rewritten in assembly for speed/size
extern "C" uint8_t __avr_cxx_personality(prog_byte *ptr, uint16_t pc_offset, void *exc,
                           personality_out *out) noexcept {
  uint8_t lp_encoding = get(ptr++);
  uint16_t lp_offset = 0;
  if (lp_encoding != DW_EH_PE_omit) {
    // in reality lp offset never seems to be set. This should really
    // consider if lp offset is set or not, but for now it works
    ptr += uleb(ptr, &lp_offset);
  }
  uint8_t type_encoding = get(ptr++);
  uint16_t types_offset = 0;
  if (type_encoding != DW_EH_PE_omit) {
    ptr += uleb(ptr, &types_offset);
  }

  prog_ptr type_table = reinterpret_cast<prog_ptr>(ptr + types_offset);
  uint8_t call_encoding = get(ptr++);
  uint16_t call_table_length;
  ptr += uleb(ptr, &call_table_length);
  if ((type_encoding != DW_EH_PE_absptr && type_encoding != DW_EH_PE_omit) ||
      call_encoding != DW_EH_PE_uleb128) {
    __avr_terminate();
  }
  prog_byte *end = ptr + call_table_length;
  uint16_t lp_ip;
  uint16_t action_offset;
  while (ptr < end) {
    uint16_t ip_start;
    ptr += uleb(ptr, &ip_start);
    uint16_t ip_range;
    ptr += uleb(ptr, &ip_range);
    ptr += uleb(ptr, &lp_ip);
    ptr += uleb(ptr, &action_offset);
    if (ip_start < pc_offset && pc_offset <= ip_start + ip_range) {
      out->landing_pad = lp_ip;
      goto found_handler;
    }
  }
  __avr_terminate();
found_handler:
  if (action_offset == 0) {
    return 0;
  }
  ptr = end + action_offset - 1;
  while (1) {
    int16_t action, offset;
    ptr += sleb(ptr, &action);
    sleb(ptr, &offset);
    ptr += offset;
    if (action != 0) {
      void *catch_type = getw(&type_table[-1 * action]);
      if (catch_type == 0) // if it is a catch(...) block, match anything
        return action;
      void *ptr = get_adjusted_ptr(exc, catch_type);
      if (ptr) {
        return action;
      }
    } else {
      return 0;
    }
    if (offset == 0)
      break;
  }
  __avr_terminate();
}

}
#endif
