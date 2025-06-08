.ident "GNU C++ x86_64 unwinding routine"
.section .note.GNU-stack,"",@progbits
.data
except_ptr: /* TODO make this multithreaded */
  .quad 0
return_values:
  .zero 16

.text
	.global	_Unwind_Resume
	.type	_Unwind_Resume, @function
	.global _Unwind_Resume_or_Rethrow /* resume_or_rethrow can be called from a forced_unwind, todo implement forced unwind maybe*/
	.type	_Unwind_Resume_or_Rethrow, @function
	.global	_Unwind_RaiseException
	.type	_Unwind_RaiseException, @function
	.global __fae_finish_unwinding
	.type __fae_finish_unwinding, @function

.unknown_func:
  ud2
.unknown_bp_reg:
  ud2
/*
Currently, this only works on linux due to the reliance on the red zone when unwinding.
To unwind, we first restore the stack pointer, then call the unwinder to load the saved
registers off of the stack. In that short moment during unwinding, an interrupt can fire
which can corrupt the reigsters. On x86 linux that isn't a problem, but on windows and
other systems a red zone may or may not exist.

In order to deal with times where red zones don't exist, data on the size of registers
allocated needs to be stored alongside the unwinder pointer. This reduces the available
bits to the unwinder pointer, which may require exotic linker scripts to maintain a small
address.

*/

_Unwind_Resume_or_Rethrow:
_Unwind_RaiseException:
  movq  %rdi, except_ptr(%rip) /* Store the exception pointer for use later */
__fae_finish_unwinding: /* This is called from the unwinder */
  popq %rdx /* load return address into first argument */
  movq except_ptr(%rip), %rsi /* load exception ptr from static storage */
  subq $40, %rsp /* allocate find_ptr_result + padding */
  movq %rsp, %rdi /*Get return out addr*/
  call __fae_get_ptr /* Call get_ptr to search the idx. entry ptr in rax, lp arg in rdx */
  movq (%rsp), %rdx /* Get entry ptr */
  movq 8(%rsp), %rcx /* Get LP ptr */
  movl 16(%rsp), %eax /* Get lp_arg */
  addq $40, %rsp /* Deallocate stack we don't need now*/
  testq %rdx, %rdx
  jz  .unknown_func /* If no entry found, trap */
  testq %rcx, %rcx
  jz .no_lp
  movq %rdx, return_values(%rip)
  movq %rsp, return_values+8(%rip)
  movl %eax, %edx /* load caught type index */
  movq except_ptr(%rip), %rax /*load exception ptr */
  jmp *%rcx /* Jump into landing pad */

/* We assume _Unwind_Resume is called from launch pads which should save
 * callee-saved registers for use, meaning we only have to restore a few
 * registers ourselves */
_Unwind_Resume:
  movq return_values(%rip), %rdx
  movq return_values(%rip), %rsp
.no_lp:
  movq (%rdx), %rcx /* Load stack */
  movq 8(%rdx), %rsi /* Load unwinder pointer */
  testq %rcx, %rcx
  js .dynamic_stack
  addq %rcx, %rsp
  jmp *%rsi
.dynamic_stack:
  cmpq $6, %rcx /* For now we only support rbp as our base pointer */
  jne .unknown_bp_reg
  movq %rbp, %rsp
  jmp *%rsi

  

