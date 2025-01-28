/* This is the unwinding routine for x86_64
   This really needs to be adjusted to fit
   allocation order which actually depends on certain things
   but for now this will do*/

.section .text

__gnu_fae_unwinder_x86_64v0:
  mov r15 qword ptr [rsp + 48]
  mov r14 qword ptr [rsp + 40]
  mov r13 qword ptr [rsp + 32]
  mov r12 qword ptr [rsp + 24]
  mov rbp qword ptr [rsp + 16]
  mov rbx qword ptr [rsp + 8]
  pop rip
  jmp __fae_finish_unwinding

