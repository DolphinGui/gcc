const char array[1] = {};
const char *__fae_table_start = array;
const char *__fae_table_stop = array;

void _Unwind_DeleteException(void) { __builtin_trap(); }
void __cxa_call_unexpected(void) { __builtin_trap(); }
void __gxx_personality_v0(void) { __builtin_trap(); }
void __fae_personality_v1(void) { __builtin_trap(); };

