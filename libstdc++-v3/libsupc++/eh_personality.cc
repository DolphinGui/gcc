// -*- C++ -*- The GNU C++ exception personality routine.
// Copyright (C) 2001-2023 Free Software Foundation, Inc.
//
// This file is part of GCC.
//
// GCC is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// GCC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

#include <bits/c++config.h>
#include <cstdlib>
#include <bits/exception_defines.h>
#include <cxxabi.h>
#include "unwind-cxx.h"


using namespace __cxxabiv1;

#include "unwind-pe.h"


struct lsda_header_info
{
  _Unwind_Ptr Start;
  _Unwind_Ptr LPStart;
  _Unwind_Ptr ttype_base;
  const unsigned char *TType;
  const unsigned char *action_table;
  unsigned char ttype_encoding;
  unsigned char call_site_encoding;
};

static const unsigned char *
parse_lsda_header (_Unwind_Context *context, const unsigned char *p,
		   lsda_header_info *info)
{
  _uleb128_t tmp;
  unsigned char lpstart_encoding;

  info->Start = (context ? _Unwind_GetRegionStart (context) : 0);

  // Find @LPStart, the base to which landing pad offsets are relative.
  lpstart_encoding = *p++;
  if (lpstart_encoding != DW_EH_PE_omit)
    p = read_encoded_value (context, lpstart_encoding, p, &info->LPStart);
  else
    info->LPStart = info->Start;

  // Find @TType, the base of the handler and exception spec type data.
  info->ttype_encoding = *p++;
  if (info->ttype_encoding != DW_EH_PE_omit)
    {
#if _GLIBCXX_OVERRIDE_TTYPE_ENCODING
      /* Older ARM EABI toolchains set this value incorrectly, so use a
	 hardcoded OS-specific format.  */
      info->ttype_encoding = _GLIBCXX_OVERRIDE_TTYPE_ENCODING;
#endif
      p = read_uleb128 (p, &tmp);
      info->TType = p + tmp;
    }
  else
    info->TType = 0;

  // The encoding and length of the call-site table; the action table
  // immediately follows.
  info->call_site_encoding = *p++;
  p = read_uleb128 (p, &tmp);
  info->action_table = p + tmp;

  return p;
}

// Return an element from a type table.

static const std::type_info *
get_ttype_entry (lsda_header_info *info, _uleb128_t i)
{
  _Unwind_Ptr ptr;

  i *= size_of_encoded_value (info->ttype_encoding);
  read_encoded_value_with_base (
#if __FDPIC__
				/* Force these flags to make sure to
				   take the GOT into account.  */
				(DW_EH_PE_pcrel | DW_EH_PE_indirect),
#else
				info->ttype_encoding,
#endif
				info->ttype_base,
				info->TType - i, &ptr);

  return reinterpret_cast<const std::type_info *>(ptr);
}

#ifdef __ARM_EABI_UNWINDER__

// The ABI provides a routine for matching exception object types.
typedef _Unwind_Control_Block _throw_typet;
#define get_adjusted_ptr(catch_type, throw_type, thrown_ptr_p) \
  (__cxa_type_match (throw_type, catch_type, false, thrown_ptr_p) \
   != ctm_failed)

// Return true if THROW_TYPE matches one if the filter types.

static bool
check_exception_spec(lsda_header_info* info, _throw_typet* throw_type,
		     void* thrown_ptr, _sleb128_t filter_value)
{
  const _uleb128_t* e = ((const _uleb128_t*) info->TType)
			  - filter_value - 1;

  while (1)
    {
      const std::type_info* catch_type;
      _uleb128_t tmp;

      tmp = *e;
      
      // Zero signals the end of the list.  If we've not found
      // a match by now, then we've failed the specification.
      if (tmp == 0)
        return false;

      tmp = _Unwind_decode_typeinfo_ptr(info->ttype_base, (_Unwind_Word) e);

      // Match a ttype entry.
      catch_type = reinterpret_cast<const std::type_info*>(tmp);

      // ??? There is currently no way to ask the RTTI code about the
      // relationship between two types without reference to a specific
      // object.  There should be; then we wouldn't need to mess with
      // thrown_ptr here.
      if (get_adjusted_ptr(catch_type, throw_type, &thrown_ptr))
	return true;

      // Advance to the next entry.
      e++;
    }
}


// Save stage1 handler information in the exception object

static inline void
save_caught_exception(struct _Unwind_Exception* ue_header,
		      struct _Unwind_Context* context,
		      void* thrown_ptr,
		      int handler_switch_value,
		      const unsigned char* language_specific_data,
		      _Unwind_Ptr landing_pad,
		      const unsigned char* action_record
			__attribute__((__unused__)))
{
    ue_header->barrier_cache.sp = _Unwind_GetGR(context, UNWIND_STACK_REG);
    ue_header->barrier_cache.bitpattern[0] = (_uw) thrown_ptr;
    ue_header->barrier_cache.bitpattern[1]
      = (_uw) handler_switch_value;
    ue_header->barrier_cache.bitpattern[2]
      = (_uw) language_specific_data;
    ue_header->barrier_cache.bitpattern[3] = (_uw) landing_pad;
}


// Restore the catch handler data saved during phase1.

static inline void
restore_caught_exception(struct _Unwind_Exception* ue_header,
			 int& handler_switch_value,
			 const unsigned char*& language_specific_data,
			 _Unwind_Ptr& landing_pad)
{
  handler_switch_value = (int) ue_header->barrier_cache.bitpattern[1];
  language_specific_data =
    (const unsigned char*) ue_header->barrier_cache.bitpattern[2];
  landing_pad = (_Unwind_Ptr) ue_header->barrier_cache.bitpattern[3];
}

#define CONTINUE_UNWINDING \
  do								\
    {								\
      if (__gnu_unwind_frame(ue_header, context) != _URC_OK)	\
	return _URC_FAILURE;					\
      return _URC_CONTINUE_UNWIND;				\
    }								\
  while (0)

// Return true if the filter spec is empty, ie throw().

static bool
empty_exception_spec (lsda_header_info *info, _Unwind_Sword filter_value)
{
  const _Unwind_Word* e = ((const _Unwind_Word*) info->TType)
			  - filter_value - 1;

  return *e == 0;
}

#else
typedef const std::type_info _throw_typet;


// Given the thrown type THROW_TYPE, pointer to a variable containing a
// pointer to the exception object THROWN_PTR_P and a type CATCH_TYPE to
// compare against, return whether or not there is a match and if so,
// update *THROWN_PTR_P.

static bool
get_adjusted_ptr (const std::type_info *catch_type,
		  const std::type_info *throw_type,
		  void **thrown_ptr_p)
{
  void *thrown_ptr = *thrown_ptr_p;

  // Pointer types need to adjust the actual pointer, not
  // the pointer to pointer that is the exception object.
  // This also has the effect of passing pointer types
  // "by value" through the __cxa_begin_catch return value.
  if (throw_type->__is_pointer_p ())
    thrown_ptr = *(void **) thrown_ptr;

  if (catch_type->__do_catch (throw_type, &thrown_ptr, 1))
    {
      *thrown_ptr_p = thrown_ptr;
      return true;
    }

  return false;
}

// Return true if THROW_TYPE matches one if the filter types.

static bool
check_exception_spec(lsda_header_info* info, _throw_typet* throw_type,
		      void* thrown_ptr, _sleb128_t filter_value)
{
  const unsigned char *e = info->TType - filter_value - 1;

  while (1)
    {
      const std::type_info *catch_type;
      _uleb128_t tmp;

      e = read_uleb128 (e, &tmp);

      // Zero signals the end of the list.  If we've not found
      // a match by now, then we've failed the specification.
      if (tmp == 0)
        return false;

      // Match a ttype entry.
      catch_type = get_ttype_entry (info, tmp);

      // ??? There is currently no way to ask the RTTI code about the
      // relationship between two types without reference to a specific
      // object.  There should be; then we wouldn't need to mess with
      // thrown_ptr here.
      if (get_adjusted_ptr (catch_type, throw_type, &thrown_ptr))
	return true;
    }
}


// Save stage1 handler information in the exception object

static inline void
save_caught_exception(struct _Unwind_Exception* ue_header,
		      struct _Unwind_Context* context
			__attribute__((__unused__)),
		      void* thrown_ptr,
		      int handler_switch_value,
		      const unsigned char* language_specific_data,
		      _Unwind_Ptr landing_pad __attribute__((__unused__)),
		      const unsigned char* action_record)
{
  __cxa_exception* xh = __get_exception_header_from_ue(ue_header);

  xh->handlerSwitchValue = handler_switch_value;
  xh->actionRecord = action_record;
  xh->languageSpecificData = language_specific_data;
  xh->adjustedPtr = thrown_ptr;

  // ??? Completely unknown what this field is supposed to be for.
  // ??? Need to cache TType encoding base for call_unexpected.
  xh->catchTemp = landing_pad;
}


// Restore the catch handler information saved during phase1.

static inline void
restore_caught_exception(struct _Unwind_Exception* ue_header,
			 int& handler_switch_value,
			 const unsigned char*& language_specific_data,
			 _Unwind_Ptr& landing_pad)
{
  __cxa_exception* xh = __get_exception_header_from_ue(ue_header);
  handler_switch_value = xh->handlerSwitchValue;
  language_specific_data = xh->languageSpecificData;
  landing_pad = (_Unwind_Ptr) xh->catchTemp;
}

#define CONTINUE_UNWINDING return _URC_CONTINUE_UNWIND

// Return true if the filter spec is empty, ie throw().

static bool
empty_exception_spec (lsda_header_info *info, _Unwind_Sword filter_value)
{
  const unsigned char *e = info->TType - filter_value - 1;
  _uleb128_t tmp;

  e = read_uleb128 (e, &tmp);
  return tmp == 0;
}

#endif // !__ARM_EABI_UNWINDER__

namespace __cxxabiv1
{

/* The ARM EABI implementation of __cxa_call_unexpected is in a
   different file so that the personality routine (PR) can be used
   standalone.  The generic routine shared datastructures with the PR
   so it is most convenient to implement it here.  */
#ifndef __ARM_EABI_UNWINDER__
extern "C" void
__cxa_call_unexpected (void *exc_obj_in)
{
  _Unwind_Exception *exc_obj
    = reinterpret_cast <_Unwind_Exception *>(exc_obj_in);

  __cxa_begin_catch (exc_obj);

  // This function is a handler for our exception argument.  If we exit
  // by throwing a different exception, we'll need the original cleaned up.
  struct end_catch_protect
  {
    end_catch_protect() { }
    ~end_catch_protect() { __cxa_end_catch(); }
  } end_catch_protect_obj;

  lsda_header_info info;
  __cxa_exception *xh = __get_exception_header_from_ue (exc_obj);
  const unsigned char *xh_lsda;
  _Unwind_Sword xh_switch_value;
  std::terminate_handler xh_terminate_handler;

  // If the unexpectedHandler rethrows the exception (e.g. to categorize it),
  // it will clobber data about the current handler.  So copy the data out now.
  xh_lsda = xh->languageSpecificData;
  xh_switch_value = xh->handlerSwitchValue;
  xh_terminate_handler = xh->terminateHandler;
  info.ttype_base = (_Unwind_Ptr) xh->catchTemp;

  __try 
    { __unexpected (xh->unexpectedHandler); } 
  __catch(...) 
    {
      // Get the exception thrown from unexpected.

      __cxa_eh_globals *globals = __cxa_get_globals_fast ();
      __cxa_exception *new_xh = globals->caughtExceptions;
      void *new_ptr = __get_object_from_ambiguous_exception (new_xh);

      // We don't quite have enough stuff cached; re-parse the LSDA.
      parse_lsda_header (0, xh_lsda, &info);

      // If this new exception meets the exception spec, allow it.
      if (check_exception_spec (&info, __get_exception_header_from_obj
                                  (new_ptr)->exceptionType,
				new_ptr, xh_switch_value))
	{ __throw_exception_again; }

      // If the exception spec allows std::bad_exception, throw that.
      // We don't have a thrown object to compare against, but since
      // bad_exception doesn't have virtual bases, that's OK; just pass 0.
#if __cpp_exceptions && __cpp_rtti
      const std::type_info &bad_exc = typeid (std::bad_exception);
      if (check_exception_spec (&info, &bad_exc, 0, xh_switch_value))
	throw std::bad_exception();
#endif   

      // Otherwise, die.
      __terminate (xh_terminate_handler);
    }
}
#endif

#if defined (__SEH__) && !defined (__USING_SJLJ_EXCEPTIONS__)
extern "C"
EXCEPTION_DISPOSITION
__gxx_personality_seh0 (PEXCEPTION_RECORD ms_exc, void *this_frame,
			PCONTEXT ms_orig_context, PDISPATCHER_CONTEXT ms_disp)
{
  return _GCC_specific_handler (ms_exc, this_frame, ms_orig_context,
				ms_disp, __gxx_personality_imp);
}
#endif /* SEH */

} // namespace __cxxabiv1
