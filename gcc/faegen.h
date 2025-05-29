#ifndef FAEGEN_H
#define FAEGEN_H

/*
 * Fae generation mostly utilizes the faegen_pass in order
 * to collect information about functions, then uses these emit
 * functions to emit the collected information into the assembly file 
 *
 * Currently, fae generation is hooked into various parts of exception
 * generation mechanism in except.cc since it so closely mirrors it, but
 * ideally fae generation and dwarf generation are abstracted out into
 * some sort of general exception information emission interface.
 *
 * Also, this currently does not handle cold sections of functions correctly.
 * Since cold function sections are counted as a seperate section rather than
 * a seperate function, any stack allocation/register pushing that occurs in
 * there is missed. That is, assuming cold sections even can allocate stack on
 * their own. More research necessary.
 *  
 */

// exactly as it looks. Emits .fae_start
extern void emit_fae_start(void);
// just like emit_fae_start, but emits .fae_end instead
extern void emit_fae_end(void);
// emits the simplified LSDA section for fae
extern void emit_fae_lsda(int section);

#endif
