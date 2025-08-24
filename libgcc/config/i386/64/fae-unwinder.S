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
  leaq (%rdi,%rdi,4), %rax /* Multiply by -5, the size of a movq instruction */
  negq %rax
  addq $.unwind_start, %rax /* I don't think relative computed jumps are a thing */
  jmp *%rax
  movq 40(%rsp), %r15
  movq 32(%rsp), %r14
  movq 24(%rsp), %r13
  movq 16(%rsp), %r12
  movq 8(%rsp), %rbp
  movq  (%rsp), %rbx
.unwind_start:
  leaq  0(,%rdi,8), %rax
  addq %rax, %rsp
  jmp __fae_finish_unwinding

/*  Yes in order to call this routine,
 *  you have to negatively offset into it*/

