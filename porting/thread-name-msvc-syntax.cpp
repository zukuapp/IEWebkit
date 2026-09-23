// Real MSVC-target Clang parses SEH here; no fake __try/__except macros.
using DWORD=unsigned long;
using ULONG_PTR=__UINTPTR_TYPE__;
using LPCSTR=const char*;
extern "C" DWORD __stdcall GetCurrentThreadId();
extern "C" void __stdcall RaiseException(DWORD,DWORD,DWORD,const ULONG_PTR*);
#define EXCEPTION_CONTINUE_EXECUTION (-1)
#define COMPILER(name) IEWK_COMPILER_##name
#define IEWK_COMPILER_MSVC 1
class Thread {
public:
    static const char* normalizeThreadName(const char*);
    static void initializeCurrentThreadEvenIfNonWTFCreated();
    static void initializeCurrentThreadInternal(const char*);
};
#include "thread-name-under-test.inc"
