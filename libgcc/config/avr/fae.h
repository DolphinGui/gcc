#pragma once

typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
typedef int int16_t;

#ifdef __FLASH
typedef const uint8_t __flash prog_byte;
#define __maybe_flash __flash
#else
typedef const uint8_t prog_byte;
#define __maybe_flash
#endif


typedef struct personality_out{
  uint16_t landing_pad;
} personality_out;

typedef uint8_t (*personality_routine)(prog_byte *ptr, uint16_t pc_offset, void *exc,
                           personality_out *lp_out);

typedef struct table_entry_t {
  uint16_t pc_begin;
  uint16_t pc_end;
  prog_byte *data;
  uint8_t frame_reg;
  uint8_t length;
  prog_byte *lsda;
  personality_routine personality;
} table_entry_t;

/* Entries are aligned by 2 so that personality_ptr
   can be read by a single movw instruction. Pad
    using 0x00. Entries may be null terminated to
    simplify debugging */

// 0x00 is padding byte, should only occur at the end of the entry.
// high bit is 0
typedef struct skip {
  uint8_t bytes;
} skip;
// skip make_skip(uint8_t b){return skip{ b & ~0b10000000};}

typedef enum reg : uint8_t {
  r2,
  r3,
  r4,
  r5,
  r6,
  r7,
  r8,
  r9,
  r10,
  r11,
  r12,
  r13,
  r14,
  r15,
  r16,
  r17,
  r28,
  r29
} reg;

// high bit is 1
typedef struct pop {
  uint8_t _reg;
} pop;

inline uint8_t get_reg(pop p) { return p._reg & ~0b10000000; }
inline pop make_pop(reg r) {
  pop result;
  result._reg = r | 0b10000000;
  return result;
}

typedef union frame_inst {
  pop p;
  skip s;
  uint8_t byte;
} frame_inst;

typedef struct table_data {
  prog_byte *data;      // r18-19
  prog_byte *data_end;  // r20-21
  uint16_t landing_pad; // r22-23
  uint8_t type_index;   // r24
  uint8_t fp_register;   // r25
} table_data;


table_data __fae_get_ptr(void *except, uint16_t pc);
