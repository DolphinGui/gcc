/* This is the unwinding routine for x86_64
   This really needs to be adjusted to fit
   allocation order which actually depends on certain things
   but for now this will do*/

.ident "GNU C++ x86_64 fae unwinder"
.section .note.GNU-stack,"",@progbits
.text
.globl __gnu_fae_unwinder_x86_64v0
.type __gnu_fae_unwinder_x86_64v0,@function

__gnu_fae_unwinder_x86_64v0:
  movq 48(%rsp), %r15
  movq 40(%rsp), %r14
  movq 32(%rsp), %r13
  movq 24(%rsp), %r12
  movq 16(%rbp), %rbp
  nop /* Padding because mov rbp is 1 byte shorter than everything else */
  movq  8(%rsp), %rbx
  jmp __fae_finish_unwinding

/*  Yes in order to call this routine,
 *  you have to negatively offset into it*/

