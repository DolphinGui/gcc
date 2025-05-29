#define INCLUDE_MEMORY
#include "backend.h"
#include "builtins.h"
#include "calls.h"
#include "cfghooks.h"
#include "cfgloop.h"
#include "cfgrtl.h"
#include "cgraph.h"
#include "common/common-target.h"
#include "config.h"
#include "coretypes.h"
#include "diagnostic.h"
#include "dwarf2.h"
#include "dwarf2asm.h"
#include "emit-rtl.h"
#include "except.h"
#include "explow.h"
#include "expmed.h"
#include "expr.h"
#include "flags.h"
#include "fold-const.h"
#include "langhooks.h"
#include "libfuncs.h"
#include "memmodel.h"
#include "optabs.h"
#include "output.h"
#include "rtl.h"
#include "stmt.h"
#include "stor-layout.h"
#include "stringpool.h"
#include "system.h"
#include "target.h"
#include "tm_p.h"
#include "tree-hash-traits.h"
#include "tree-pass.h"
#include "tree-pretty-print.h"
#include "tree.h"

#include "faegen.h"

#include <cassert>
#include <cstdio>

struct GTY(()) function_data {
  const char *name = NULL;
  unsigned regs = 0;
  unsigned stack_usage = 0;
  unsigned num = 0;
  const char *lsda_label = NULL;
  int sp_reg = 0;
  bool used_alloca = false;
};

static GTY(()) function_data cur_fun_dat = {};

static void output_delta(const char *a, const char *b) {
  fprintf(asm_out_file, "\t.word ");
  assemble_name(asm_out_file, a);
  fprintf(asm_out_file, " - ");
  assemble_name(asm_out_file, b);
  fputc('\n', asm_out_file);
}

static int lsda_base = 0;
static const char *create_lsda_label(const char *prefix) {
  char label[32] = {};
  ASM_GENERATE_INTERNAL_LABEL(label, prefix, lsda_base);
  return ggc_strdup(label);
}

static void output_ttype(tree type) {
  rtx value;

  if (type == NULL_TREE)
    value = const0_rtx;
  else {
    if (TYPE_P(type))
      type = lookup_type_for_runtime(type);

    value = expand_expr(type, NULL_RTX, VOIDmode, EXPAND_INITIALIZER);

    STRIP_NOPS(type);
    if (TREE_CODE(type) == ADDR_EXPR) {
      type = TREE_OPERAND(type, 0);
    } else
      gcc_assert(TREE_CODE(type) == INTEGER_CST);
  }

  if (targetm.asm_out.ttype(value))
    return;

  assemble_integer(value, 4, 4 * BITS_PER_UNIT, 1);
}

// basically copied from except.cc
static void switch_to_fae_lsda_section(const char *fnname);

// void emit_header(int regions);
static void emit_regions(int regions, int section, int base);
static void emit_action_records(int regions, int section, int base);

void emit_fae_lsda(int section) {
  gcc_assert(cur_fun_dat.name != NULL);
  const int base =
      call_site_base - crtl->eh.call_site_record_v[section]->length();

  int regions = crtl->eh.call_site_record_v[section]->length();

  switch_to_fae_lsda_section(cur_fun_dat.name);

  fputs("# call sites\n", asm_out_file);
  cur_fun_dat.lsda_label = create_lsda_label("FAElsda");
  ASM_OUTPUT_LABEL(asm_out_file, cur_fun_dat.lsda_label);

  emit_regions(regions, section, base);

  fprintf(asm_out_file, "# action records\n");

  emit_action_records(regions, section, base);

  auto l = create_lsda_label("FAEttypes");
  ASM_OUTPUT_LABEL(asm_out_file, l);
  int ttypes = vec_safe_length(cfun->eh->ttype_data);
  if (ttypes)
    assemble_align(BITS_PER_UNIT * 4);
  for (int i = 0; i < ttypes; ++i) {
    tree type = (*cfun->eh->ttype_data)[i];
    output_ttype(type);
  }
  lsda_base += 1;
}

uint fnum = 0;

const pass_data fae_data = {
    RTL_PASS,      /* type */
    "fae_gen",     /* name */
    OPTGROUP_NONE, /* optinfo_flags */
    TV_FINAL,      /* tv_id */
    0,             /* properties_required */
    0,             /* properties_provided */
    0,             /* properties_destroyed */
    0,             /* todo_flags_start */
    0,             /* todo_flags_finish */
};

struct GTY(()) PassFae final : rtl_opt_pass {

  PassFae(gcc::context *ctxt) : rtl_opt_pass(fae_data, ctxt), context(ctxt) {}
  ~PassFae() override {}

  unsigned int execute(function *fun) override;
  bool gate(function *fun) override;
  opt_pass *clone() override { return new PassFae(context); }

  gcc::context *context;
};

rtl_opt_pass *make_pass_faegen(gcc::context *ct) { return new PassFae(ct); }

void emit_fae_start() {
  gcc_assert(asm_out_file);
  gcc_assert(cur_fun_dat.name != NULL);
  fprintf(asm_out_file, "\t.fae_start\n");
}

/* Currently this does not correctly parse cold/hot sections, so
any exceptions thrown in destructors or such won't work*/
void emit_fae_end() {
  gcc_assert(asm_out_file);
  if (cur_fun_dat.used_alloca)
    gcc_assert(cur_fun_dat.regs <= 5);
  else
    gcc_assert(cur_fun_dat.regs <= 6);

  const char *unwinder = cur_fun_dat.used_alloca
                             ? "\t.fae_unwinder __gnu_fae_unwinder_x86_64_dynv0 - "
                             : "\t.fae_unwinder __gnu_fae_unwinder_x86_64v0 - ";

  fputs(unwinder, asm_out_file);
  // x86_64 mov is 5 bytes long.
  fprint_whex(asm_out_file, cur_fun_dat.regs * 5);
  fputc('\n', asm_out_file);

  if (!cur_fun_dat.used_alloca) {
    fputs("\t.fae_stacksize ", asm_out_file);
    fprint_whex(asm_out_file, cur_fun_dat.stack_usage);
    fputc('\n', asm_out_file);
  } else {
    fputs("\t.fae_save_sp ", asm_out_file);
    fprint_ul(asm_out_file, cur_fun_dat.sp_reg);
    fputc('\n', asm_out_file);
  }

  if (cur_fun_dat.lsda_label) {
    fputs("\t.fae_handlerdata ", asm_out_file);
    assemble_name(asm_out_file, cur_fun_dat.lsda_label);
    fputc('\n', asm_out_file);
  }

  fprintf(asm_out_file, "\t.fae_end\n");
}

static void assert_or_set(int expr, int &value) {
  if (value == 0) {
    value = expr;
  } else {
    gcc_assert(value == expr);
  }
}

// todo use machine_frame info instead of stupid parsing
// also need to handle floating point stack seperately
unsigned int PassFae::execute(function *f) {
  gcc_assert(DECL_ASSEMBLER_NAME_SET_P(f->decl));

  long stack = DEFAULT_INCOMING_FRAME_SP_OFFSET;
  int regs = 0;
  int reg_length = 0;
  int saved_sp = 0;
  bool has_saved_stack = false;

  for (rtx_insn *rtx = get_insns(); rtx; rtx = NEXT_INSN(rtx)) {
    rtx_code code = GET_CODE(rtx);
    // we are not interested in code body or epilogue
    if (code == NOTE && NOTE_KIND(rtx) == NOTE_INSN_PROLOGUE_END) {
      break;
    }
    // only interested in frame manipulation instructions
    if (!RTX_FLAG(rtx, frame_related) || code != INSN) {
      continue;
    }

    auto *inner = PATTERN(rtx);

    auto in_code = GET_CODE(inner);
    if (in_code == SET) {
      auto reg = XEXP(inner, 1);
      assert(GET_CODE(reg) == REG);

      if (REGNO(reg) == STACK_POINTER_REGNUM) {
        has_saved_stack = true;
        saved_sp = REGNO(XEXP(inner, 0));
        regs -= 1; // This means that BP has been saved
      } else {
        regs += 1;
        auto regmode = GET_MODE_SIZE(GET_MODE(reg));
        uint regsize;
        gcc_assert(regmode.is_constant(&regsize));
        assert_or_set(regsize, reg_length);
        stack += regsize;
      }

    } else if (in_code == PARALLEL) {
      // usually stack allocation is parallel because it clobbers stuff, but
      // this needs testing for each platform
      assert(XVECLEN(inner, 0) > 0);
      auto alloc = XVECEXP(inner, 0, 0);
      {
        auto sp = XEXP(alloc, 0);
        // ensuring we're adding to stack pointer
        assert(GET_CODE(sp) == REG && XINT(sp, 0) == 7);
      }
      auto set_stack = XEXP(alloc, 1);
      assert(GET_CODE(set_stack) == PLUS);
      {
        auto sp = XEXP(alloc, 0);
        assert(GET_CODE(sp) == REG && XINT(sp, 0) == 7);
      }
      auto operand = XEXP(set_stack, 1);
      assert(GET_CODE(operand) == CONST_INT);
      // stack grows downwards
      stack += XWINT(operand, 0) * -1;
    }
  }

  if (f->calls_alloca) {
    gcc_assert(has_saved_stack);
  }

  cur_fun_dat.name = IDENTIFIER_POINTER(DECL_ASSEMBLER_NAME(f->decl));
  cur_fun_dat.regs = regs;
  cur_fun_dat.stack_usage = stack;
  cur_fun_dat.num = fnum++;
  cur_fun_dat.used_alloca = has_saved_stack;
  cur_fun_dat.sp_reg = saved_sp;

  return 0;
}

bool PassFae::gate(function *) {
  if (!targetm.have_prologue()) {
    printf("this function has no prologue\n");
    return false;
  }
  return true;
}

void switch_to_fae_lsda_section(const char *fnname) {
  int flags = SECTION_WRITE;
  section *s;
  if (EH_TABLES_CAN_BE_READ_ONLY) {
    int tt_format = ASM_PREFERRED_EH_DATA_FORMAT(0, 1);
    flags = ((!flag_pic || ((tt_format & 0x70) != DW_EH_PE_absptr &&
                            (tt_format & 0x70) != DW_EH_PE_aligned))
                 ? 0
                 : SECTION_WRITE);
  }

  // targetm_common.have_named_sections; assert this later, for some reason
  // can't access

#ifdef HAVE_LD_EH_GC_SECTIONS
  if (flag_function_sections ||
      (DECL_COMDAT_GROUP(current_function_decl) && HAVE_COMDAT_GROUP)) {
    char *section_name = XNEWVEC(char, strlen(fnname) + 32);
    if (DECL_COMDAT_GROUP(current_function_decl) && HAVE_COMDAT_GROUP)
      flags |= SECTION_LINKONCE;
    sprintf(section_name, ".fae.lsda.%s", fnname);
    s = get_section(section_name, flags, current_function_decl);
    free(section_name);
  } else
#endif
    s = get_section(".fae.lsda", flags, NULL);
  switch_to_section(s);
}

void emit_regions(int regions, int section, int base) {
  const char *func_begin;
  if (section == 0)
    func_begin = current_function_func_begin_label;
  else if (first_function_block_is_cold)
    func_begin = crtl->subsections.hot_section_label;
  else
    func_begin = crtl->subsections.cold_section_label;

  int action_check_num = 0;
  for (int i = 0; i < regions; ++i) {
    char start[32];
    char end[32];
    char landing_pad[32];
    auto &callsite = *(*crtl->eh.call_site_record_v[section])[i];
    ASM_GENERATE_INTERNAL_LABEL(start, "LEHB", base + i);
    ASM_GENERATE_INTERNAL_LABEL(end, "LEHE", base + i);
    if (callsite.landing_pad)
      ASM_GENERATE_INTERNAL_LABEL(landing_pad, "L",
                                  CODE_LABEL_NUMBER(callsite.landing_pad));

    output_delta(start, func_begin);
    output_delta(end, start);
    if (callsite.landing_pad)
      output_delta(landing_pad, func_begin);
    else
      fputs("\t.word 0\n", asm_out_file);

    if (callsite.action != 0) {
      char action_label[32] = {};
      ASM_GENERATE_INTERNAL_LABEL(action_label, "FAEaction_record",
                                  (action_check_num << 16) + base);

      output_delta(action_label, cur_fun_dat.lsda_label);
      action_check_num += 1;
    } else {
      fputs("\t.word 0\n", asm_out_file);
    }
  }
}

static vec<uint32_t> parse_action(uchar *actions, uint begin);

void emit_action_records(int regions, int section, int base) {

  vec<vec<uint32_t>> action_checks;
  action_checks.create(4);

  for (int i = 0; i < regions; ++i) {
    int action = (*crtl->eh.call_site_record_v[section])[i]->action;
    if (action != 0) {
      auto actions = parse_action(crtl->eh.action_record_data->begin(), action);
      action_checks.safe_push(std::move(actions));
    }
  }

  int action_check_num = 0;
  for (auto &check : action_checks) {
    fprintf(asm_out_file, "# new action check\n");

    char action_label[32] = {};
    ASM_GENERATE_INTERNAL_LABEL(action_label, "FAEaction_record",
                                (action_check_num << 16) + base);
    ASM_OUTPUT_LABEL(asm_out_file, action_label);
    ++action_check_num;
    for (auto filter : check) {
      fputs("\t.byte ", asm_out_file);
      fprint_whex(asm_out_file, filter);
      fputc('\n', asm_out_file);
    }
  }
}

typedef signed long long _sleb128_t;
typedef unsigned long long _uleb128_t;

// Literally copied verbatim from unwind-pe.h
// WET is kinda bad, but trying to include
// unwind-pe.h is worse.
static const unsigned char *read_sleb128(const unsigned char *p,
                                         _sleb128_t *val) {
  unsigned int shift = 0;
  unsigned char byte;
  _uleb128_t result;

  result = 0;
  do {
    byte = *p++;
    result |= ((_uleb128_t)byte & 0x7f) << shift;
    shift += 7;
  } while (byte & 0x80);

  /* Sign-extend a negative value.  */
  if (shift < 8 * sizeof(result) && (byte & 0x40) != 0)
    result |= -(((_uleb128_t)1L) << shift);

  *val = (_sleb128_t)result;
  return p;
}

static const unsigned char *read_uleb128(const unsigned char *p,
                                         _uleb128_t *val) {
  unsigned int shift = 0;
  unsigned char byte;
  _uleb128_t result;

  result = 0;
  do {
    byte = *p++;
    result |= ((_uleb128_t)byte & 0x7f) << shift;
    shift += 7;
  } while (byte & 0x80);

  *val = result;
  return p;
}

vec<uint32_t> parse_action(uchar *actions, uint begin) {
  _uleb128_t ttype = 0;
  _sleb128_t offset = begin;
  vec<uint32_t> ttypes;
  ttypes.create(8);
  do {
    auto current = read_uleb128(actions + offset - 1, &ttype);
    _sleb128_t next;
    read_sleb128(current, &next);
    offset += next + 1;
    // who would create more than 2^32 try catches anyways?
    gcc_assert(next < 0xffff'ffff);
    ttypes.safe_push(ttype);
  } while (ttype != 0);
  return ttypes;
}

#include "gt-faegen.h"
