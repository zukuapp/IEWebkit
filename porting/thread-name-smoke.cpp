#ifdef IEWK_REAL_WIN32
#include <windows.h>
#include "probe-telemetry.h"
#else
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <initializer_list>
using DWORD=uint32_t;
using ULONG_PTR=uintptr_t;
using LPCSTR=const char*;
static unsigned stage;
static const char* expected;
static void OutputDebugStringA(const char* value) {
    assert(stage==1 && value==expected); stage=2;
}
#endif
#define COMPILER(name) IEWK_COMPILER_##name
#define IEWK_COMPILER_MSVC 0
static unsigned normalized, initialized;
class Thread {
public:
    static const char* normalizeThreadName(const char* name) {
        ++normalized;
#ifndef IEWK_REAL_WIN32
        assert(stage==0); stage=1;
        return expected;
#else
        return name;
#endif
    }
    static void initializeCurrentThreadEvenIfNonWTFCreated() {
        ++initialized;
#ifndef IEWK_REAL_WIN32
        assert(stage==2); stage=3;
#endif
    }
    static void initializeCurrentThreadInternal(const char*);
};
#include "thread-name-under-test.inc"
int main() {
#ifdef IEWK_REAL_WIN32
    probe_open("C:\\NAMEDIAG.LOG");
    Thread::initializeCurrentThreadInternal("IEWK.NameProbe");
    probe_log("normalized",normalized); probe_log("initialized",initialized);
    return probe_finish(normalized==1 && initialized==1 ? 0 : 10);
#else
    for (const char* name : {"worker", "", "already-normalized"}) {
        stage=0; expected="normalized-result";
        Thread::initializeCurrentThreadInternal(name);
        assert(stage==3);
    }
    assert(normalized==3 && initialized==3);
    puts("Thread name: normalization/output/initialization ordering passed for 3 cases");
#endif
}
