/* The presence of alloca means that the base pointer
   is not present, messing up the allocation order
   necessitating another unwinder routine */

.ident "GNU C++ x86_64 fae unwinder for dynamic stack"
.section .note.GNU-stack,"",@progbits
.text
.globl __gnu_fae_unwinder_x86_64_dynv0
.type __gnu_fae_unwinder_x86_64_dynv0,@function

 __gnu_fae_unwinder_x86_64_dynv0:
  movq 48(%rsp), %r15
  movq 40(%rsp), %r14
  movq 32(%rsp), %r13
  movq 24(%rsp), %r12
  movq  8(%rsp), %rbx
  pop %rbp
  jmp __fae_finish_unwinding


