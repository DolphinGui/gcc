#define INCLUDE_MEMORY
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "cfghooks.h"
#include "tree-pass.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "expmed.h"
#include "optabs.h"
#include "emit-rtl.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "explow.h"
#include "stmt.h"
#include "expr.h"
#include "calls.h"
#include "libfuncs.h"
#include "except.h"
#include "output.h"
#include "dwarf2asm.h"
#include "dwarf2.h"
#include "common/common-target.h"
#include "langhooks.h"
#include "cfgrtl.h"
#include "tree-pretty-print.h"
#include "cfgloop.h"
#include "builtins.h"
#include "tree-hash-traits.h"
#include "flags.h"
#include "faegen.h"

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

static const char *create_lsda_label(const char *prefix) {
  char label[32] = {};
  ASM_GENERATE_INTERNAL_LABEL(label, prefix, cfun->funcdef_no);
  return ggc_strdup(label);
}

static void output_ttype(tree type) {
 rtx value;
  bool is_public = true;

  if (type == NULL_TREE)
    value = const0_rtx;
  else
    {
      /* FIXME lto.  pass_ipa_free_lang_data changes all types to
	 runtime types so TYPE should already be a runtime type
	 reference.  When pass_ipa_free_lang data is made a default
	 pass, we can then remove the call to lookup_type_for_runtime
	 below.  */
      if (TYPE_P (type))
	type = lookup_type_for_runtime (type);

      value = expand_expr (type, NULL_RTX, VOIDmode, EXPAND_INITIALIZER);

      /* Let cgraph know that the rtti decl is used.  Not all of the
	 paths below go through assemble_integer, which would take
	 care of this for us.  */
      STRIP_NOPS (type);
      if (TREE_CODE (type) == ADDR_EXPR)
	{
	  type = TREE_OPERAND (type, 0);
	  if (VAR_P (type))
	    is_public = TREE_PUBLIC (type);
	}
      else
	gcc_assert (TREE_CODE (type) == INTEGER_CST);
    }

  /* Allow the target to override the type table entry format.  */
  if (targetm.asm_out.ttype (value))
    return;
  int tt_format = ASM_PREFERRED_EH_DATA_FORMAT (/*code=*/0, /*global=*/1);
  int tt_format_size = size_of_encoded_value(tt_format);
  if (tt_format == DW_EH_PE_absptr || tt_format == DW_EH_PE_aligned)
    assemble_integer (value, tt_format_size,
		      tt_format_size * BITS_PER_UNIT, 1);
  else
    dw2_asm_output_encoded_addr_rtx (tt_format, value, is_public, NULL);
}

// basically copied from except.cc
static void switch_to_fae_lsda_section(const char *fnname);

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
void emit_fae_end(int is_end) {
  gcc_assert(asm_out_file);
  if (cur_fun_dat.used_alloca)
    gcc_assert(cur_fun_dat.regs <= 5);
  else
    gcc_assert(cur_fun_dat.regs <= 6);

  const char *unwinder = cur_fun_dat.used_alloca
                             ? "\t.fae_unwinder __gnu_fae_unwinder_x86_64_dynv0 + "
                             : "\t.fae_unwinder __gnu_fae_unwinder_x86_64v0 + ";
  const int unwind_offset = (cur_fun_dat.used_alloca ? 5 : 6) - cur_fun_dat.regs;
  gcc_assert(unwind_offset >= 0);
  fputs(unwinder, asm_out_file);
  // x86_64 mov is 5 bytes long.
  fprint_ul(asm_out_file, unwind_offset * 5);
  fputc('\n', asm_out_file);

  if (!cur_fun_dat.used_alloca) {
    fputs("\t.fae_stacksize ", asm_out_file);
    fprint_ul(asm_out_file, cur_fun_dat.stack_usage);
    fputc('\n', asm_out_file);
  } else {
    fputs("\t.fae_save_sp ", asm_out_file);
    fprint_ul(asm_out_file, cur_fun_dat.sp_reg);
    fputc('\n', asm_out_file);
  }
  
  if (crtl->uses_eh_lsda) {
    bool is_cold = is_end && crtl->has_bb_partition;
    fputs("\t.fae_handlerdata ", asm_out_file); 
    assemble_name(asm_out_file, create_lsda_label(is_cold ? "FAE2lsda" : "FAElsda"));
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

static bool is_callee_saved(int regno);
static void check_rtx(rtx_def *inner, int &regs, long &stack, int &saved_sp,
                      bool &has_saved_stack);

static const char *format(rtx_code r);
// todo use machine_frame info instead of stupid parsing
// also need to handle floating point stack seperately
unsigned int PassFae::execute(function *f) {
  gcc_assert(DECL_ASSEMBLER_NAME_SET_P(f->decl));

  long stack = 0;
  int regs = 0;
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
    check_rtx(inner, regs, stack, saved_sp, has_saved_stack);
  }

  if (f->calls_alloca) {
    gcc_assert(has_saved_stack);
  }

  cur_fun_dat.name = IDENTIFIER_POINTER(DECL_ASSEMBLER_NAME(f->decl));
  cur_fun_dat.regs = regs;
  cur_fun_dat.stack_usage = stack;
  cur_fun_dat.num = fnum++;
  cur_fun_dat.used_alloca = f->calls_alloca;
  cur_fun_dat.sp_reg = saved_sp;

  return 0;
}

bool is_callee_saved(int regno) {
  int regs[] = {3, 6, 40, 41, 42, 43};
  for (int i = 0; i < 6; ++i) {
    if (regno == regs[i])
      return true;
  }
  return false;
}

int reg_length = 0;
void check_rtx(rtx_def *inner, int &regs, long &stack, int &saved_sp,
               bool &has_saved_stack) {
  auto in_code = GET_CODE(inner);
  if (in_code == SET) {
    auto dst = XEXP(inner, 0);
    auto src = XEXP(inner, 1);
    if (GET_CODE(dst) == MEM) {
      // We must be saving a register to stack
      gcc_assert(GET_CODE(src) == REG);
      if (is_callee_saved(REGNO(src)))
        regs += 1;
      auto regmode = GET_MODE_SIZE(GET_MODE(src));
      uint regsize;
      gcc_assert(regmode.is_constant(&regsize));
      assert_or_set(regsize, reg_length);
      stack += regsize;
    } else {
      // we must be either incrementing the stack pointer
      // or saving it to base pointer
      gcc_assert(GET_CODE(dst) == REG);
      // register to register transfer must be stack to base save
      if (GET_CODE(src) == REG) {
        gcc_assert(REGNO(src) == STACK_POINTER_REGNUM);
        has_saved_stack = true;
        saved_sp = REGNO(XEXP(inner, 0));
        regs -= 1;
      } else {

        gcc_assert(REGNO(dst) == STACK_POINTER_REGNUM);
        gcc_assert(GET_CODE(src) == PLUS);
        auto first_op = XEXP(src, 0);
        gcc_assert(GET_CODE(first_op) == REG &&
                   REGNO(first_op) == STACK_POINTER_REGNUM);
        auto second_op = XEXP(src, 1);
        gcc_assert(GET_CODE(second_op) == CONST_INT);
        stack += -1 * XWINT(second_op, 0);
      }
    }
  } else if (in_code == PARALLEL) {
    // parse PARALLEL recursively since sometimes push clobber registers
    int len = XVECLEN(inner, 0);
    for (int i = 0; i < len; ++i) {
      check_rtx(XVECEXP(inner, 0, i), regs, stack, saved_sp, has_saved_stack);
    }
  }
}

bool PassFae::gate(function *) {
  if (!targetm.have_prologue()) {
    printf("this function has no prologue\n");
    return false;
  }
  return true;
}

static void emit_header(int regions, const char *ttypes);
static void emit_regions(int regions, int section, int base);
static void emit_action_records(int regions, int section, int base);

void emit_fae_lsda(int section) {
  gcc_assert(cur_fun_dat.name != NULL);
  const int regions = vec_safe_length(crtl->eh.call_site_record_v[section]);

  const int base = call_site_base - regions;

  switch_to_fae_lsda_section(cur_fun_dat.name);

  auto l = create_lsda_label(section ? "FAE2ttypes" : "FAEttypes");
  cur_fun_dat.lsda_label = create_lsda_label(section ? "FAE2lsda" : "FAElsda");

  emit_header(regions, l);

  emit_regions(regions, section, base);
  emit_action_records(regions, section, base);

  int ttypes = vec_safe_length(cfun->eh->ttype_data);
  if (ttypes)
    assemble_align(BITS_PER_UNIT * 4);
  ASM_OUTPUT_LABEL(asm_out_file, l);
  for (int i = 0; i < ttypes; ++i) {
    tree type = (*cfun->eh->ttype_data)[i];
    output_ttype(type);
  }
}

void emit_header(int regions, const char *ttypes) {
  ASM_OUTPUT_LABEL(asm_out_file, cur_fun_dat.lsda_label);
  fputs("\t.quad __fae_cpp_personality1\n", asm_out_file); 
  fputs("\t.word ", asm_out_file);
  fprint_ul(asm_out_file, regions);
  fputc('\n', asm_out_file);
  output_delta(ttypes, cur_fun_dat.lsda_label);
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
    output_delta(end, func_begin);
    if (callsite.landing_pad)
      output_delta(landing_pad, func_begin);
    else
      fputs("\t.word 0\n", asm_out_file);

    if (callsite.action != 0) {
      char action_label[32] = {};
      gcc_assert(base < 1000);
      ASM_GENERATE_INTERNAL_LABEL(action_label, "FAEaction_record",
                                  action_check_num * 1000 + base);

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
  if(regions)
  for (auto &check : action_checks) {
    char action_label[32] = {};
    gcc_assert(base < 1000);
    ASM_GENERATE_INTERNAL_LABEL(action_label, "FAEaction_record",
                                action_check_num * 1000 + base);
    ASM_OUTPUT_LABEL(asm_out_file, action_label);
    ++action_check_num;
    for (auto filter : check) {
      fputs("\t.byte ", asm_out_file);
      fprint_ul(asm_out_file, filter);
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
