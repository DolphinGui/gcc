.ident "GNU C++ x86_64 unwinding routine"

.data
except_ptr:
  .zero 8

.text
	.global	_Unwind_Resume
	.type	_Unwind_Resume, @function
	.global _Unwind_Resume_or_Rethrow /* resume_or_rethrow can be called from a forced_unwind, todo implement forced unwind maybe*/
	.type	_Unwind_Resume_or_Rethrow, @function
	.global	_Unwind_RaiseException
	.type	_Unwind_RaiseException, @function

unknown_func:
  call __gxx_x86_terminate

_Unwind_Resume_or_Rethrow:
_Unwind_RaiseException:
  movq %rdi, except_ptr(%rip)
.tailcall:
  popq %rax
  movq except_ptr(%rip), %rdi
  call __fae_get_ptr



