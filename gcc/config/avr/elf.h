/* Copyright (C) 2011-2024 Free Software Foundation, Inc.
   Contributed by Georg-Johann Lay (avr@gjlay.de)

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.
   
   GCC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */


/* Overriding some definitions from elfos.h for AVR.  */

#undef PCC_BITFIELD_TYPE_MATTERS

#undef MAX_OFILE_ALIGNMENT
#define MAX_OFILE_ALIGNMENT (32768 * 8)

#undef STRING_LIMIT
#define STRING_LIMIT ((unsigned) 64)

#undef  ASM_DECLARE_FUNCTION_NAME
#define ASM_DECLARE_FUNCTION_NAME avr_asm_declare_function_name
#undef  ASM_DECLARE_FUNCTION_SIZE
#define ASM_DECLARE_FUNCTION_SIZE(FILE, FNAME, DECL)     \
  do                       \
    {                      \
      AVR_OUTPUT_FN_UNWIND (FILE, FALSE);       \
      if (!flag_inhibit_size_directive)            \
   ASM_OUTPUT_MEASURED_SIZE (FILE, FNAME);         \
    }                      \
  while (0)
/* Be conservative in crtstuff.c.  */

