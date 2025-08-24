/* The presence of alloca means that the base pointer
   is not present, messing up the allocation order
   necessitating another unwinder routine */

.ident "GNU C++ x86_64 fae unwinder for dynamic stack"
.section .note.GNU-stack,"",@progbits
.text
.globl __gnu_fae_unwinder_x86_64_dynv0
.type __gnu_fae_unwinder_x86_64_dynv0,@function

__gnu_fae_unwinder_x86_64_dynv0:
  leaq (,%rdi,4), %rax /* Multiply by -4, the size of a movq instruction */
  negq %rax
  addq $.unwind_start, %rax /* I don't think relative computed jumps are a thing */
  jmp *%rax
  movq -32(%rbp), %r15 /* get registers indexed by bp instead of sp */
  movq -24(%rbp), %r14
  movq -16(%rbp), %r13
  movq -8(%rbp), %r12
  movq  (%rbp), %rbx
.unwind_start:
  movq %rbp, %rsp
  popq %rbp
  jmp __fae_finish_unwinding

