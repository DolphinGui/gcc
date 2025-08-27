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

static void output_delta_uleb(const char *a, const char *b) {
  fprintf(asm_out_file, "\t.uleb128 ");
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

void emit_fae_start(bool) {
  if(TREE_NOTHROW(current_function_decl))
    return;
  fprintf(asm_out_file, "\t.fae_start\n");
}

/* Currently this does not correctly parse cold/hot sections, so
any exceptions thrown in destructors or such won't work*/
void emit_fae_end(bool is_end) {
  if(TREE_NOTHROW(current_function_decl))
    return;
  if (cur_fun_dat.used_alloca)
    gcc_assert(cur_fun_dat.regs <= 5);
  else
    gcc_assert(cur_fun_dat.regs <= 6);

  const char *unwinder =
      cur_fun_dat.used_alloca
          ? "\t.fae_unwinder __gnu_fae_unwinder_x86_64_dynv0, "
          : "\t.fae_unwinder __gnu_fae_unwinder_x86_64v0, ";
  fputs(unwinder, asm_out_file);
  fprint_ul(asm_out_file, cur_fun_dat.regs);
  fputc('\n', asm_out_file);

  // Hardcoding DWARF labels is kinda bad, but I haven't seen anyone
  // override them yet
  /*
  fputs("\t.fae_fsize .LFB", asm_out_file);
  fprint_ul(asm_out_file, current_function_funcdef_no);
  fputs(" - .LFE", asm_out_file);
  fprint_ul(asm_out_file, current_function_funcdef_no);
  fputc('\n', asm_out_file);*/
  // Commenting out for now, once the proof of concept is done
  // I'll get to this later

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
    assemble_name(asm_out_file,
                  create_lsda_label(is_cold ? "FAE2lsda" : "FAElsda"));
    fputc('\n', asm_out_file);
  }

  fputs("\t.fae_end\n", asm_out_file);
}

// todo use machine_frame info instead of stupid parsing
// also need to handle floating point stack seperately
unsigned int PassFae::execute(function *f) {
  gcc_assert(DECL_ASSEMBLER_NAME_SET_P(f->decl));

  // todo make a macro that makes this machine specific, for now I'm hardcoding
  // x86_64 machine struct for testing but I need to port this for arm
  cur_fun_dat.name = IDENTIFIER_POINTER(DECL_ASSEMBLER_NAME(f->decl));
  cur_fun_dat.regs = UNWIND_REGISTERS_SAVED;
  cur_fun_dat.stack_usage = UNWIND_STACK_USED;
  cur_fun_dat.num = fnum++;
  cur_fun_dat.used_alloca = frame_pointer_needed;
  cur_fun_dat.sp_reg = 6;
  return 0;
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

    output_delta_uleb(start, func_begin);
    output_delta_uleb(end, func_begin);
    if (callsite.landing_pad)
      output_delta_uleb(landing_pad, func_begin);
    else
      fputs("\t.uleb128 0\n", asm_out_file);

    if (callsite.action != 0) {
      char action_label[32] = {};
      gcc_assert(base < 1000);
      ASM_GENERATE_INTERNAL_LABEL(action_label, "FAEaction_record",
                                  action_check_num * 1000 + base);

      output_delta_uleb(action_label, cur_fun_dat.lsda_label);
      action_check_num += 1;
    } else {
      fputs("\t.uleb128 0\n", asm_out_file);
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
  if (regions)
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
