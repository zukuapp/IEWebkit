#include <cassert>
#include <cstdint>
#include <cstdio>
#ifdef IEWK_REAL_WIN32
#include <windows.h>
#include "probe-telemetry.h"
#define RELEASE_ASSERT(value) do { if (!(value)) ExitProcess(50); } while (0)
static void* currentStackPointer() {
    uintptr_t pointer;
#if defined(__i386__)
    __asm__ volatile("mov %%esp,%0" : "=r"(pointer));
#else
    __asm__ volatile("mov %%rsp,%0" : "=r"(pointer));
#endif
    return reinterpret_cast<void*>(pointer);
}
#else
#include <vector>
using DWORD = uint32_t;
using ULONG_PTR = uintptr_t;
constexpr DWORD MEM_COMMIT=0x1000, MEM_RESERVE=0x2000, MEM_FREE=0x10000, MEM_PRIVATE=0x20000;
constexpr DWORD PAGE_NOACCESS=1, PAGE_READONLY=2, PAGE_READWRITE=4, PAGE_WRITECOPY=8;
constexpr DWORD PAGE_EXECUTE_READWRITE=0x40, PAGE_EXECUTE_WRITECOPY=0x80, PAGE_GUARD=0x100;
struct NT_TIB { void* StackBase; };
struct SYSTEM_INFO { DWORD dwPageSize; };
struct MEMORY_BASIC_INFORMATION {
    void* BaseAddress; void* AllocationBase; uintptr_t RegionSize; DWORD State, Protect, Type;
};
struct Rejected { };
#define RELEASE_ASSERT(value) do { if (!(value)) throw Rejected{}; } while (0)
static NT_TIB tib;
static uintptr_t sp;
static DWORD pageSize;
static std::vector<MEMORY_BASIC_INFORMATION> regions;
static int queries, failQuery, modernCalls;
static bool shortQuery, nullTib;
static NT_TIB* NtCurrentTeb() { return nullTib ? nullptr : &tib; }
static void* currentStackPointer() { return reinterpret_cast<void*>(sp); }
static void GetSystemInfo(SYSTEM_INFO* info) { info->dwPageSize=pageSize; }
static uintptr_t VirtualQuery(void* address, MEMORY_BASIC_INFORMATION* out, uintptr_t) {
    if (++queries == failQuery) return shortQuery ? sizeof(*out)-1 : 0;
    uintptr_t value=reinterpret_cast<uintptr_t>(address);
    for (auto& region : regions) {
        uintptr_t start=reinterpret_cast<uintptr_t>(region.BaseAddress);
        if (value >= start && value-start < region.RegionSize) { *out=region; return sizeof(*out); }
    }
    return 0;
}
static void GetCurrentThreadStackLimits(ULONG_PTR* low, ULONG_PTR* high) {
    ++modernCalls; *low=0x101000; *high=0x110000;
}
#endif
class StackBounds {
public:
    void* origin; void* bound;
    static StackBounds currentThreadStackBoundsInternal();
};
#include "stack-bounds-under-test.inc"

#ifdef IEWK_REAL_WIN32
static bool within(StackBounds bounds) {
    uintptr_t pointer=reinterpret_cast<uintptr_t>(currentStackPointer());
    return reinterpret_cast<uintptr_t>(bounds.bound)<pointer
        && pointer<reinterpret_cast<uintptr_t>(bounds.origin);
}
__attribute__((noinline)) static bool grow(unsigned depth, StackBounds before) {
    volatile char padding[8192]; padding[0]=1; padding[sizeof(padding)-1]=2;
    auto current=StackBounds::currentThreadStackBoundsInternal();
    bool result=within(current) && current.origin==before.origin && current.bound==before.bound;
    if (depth) result = grow(depth-1,before) && result;
    return result && padding[0]+padding[sizeof(padding)-1]==3;
}
static DWORD WINAPI worker(void*) {
    auto bounds=StackBounds::currentThreadStackBoundsInternal();
    return within(bounds) && grow(4,bounds) ? 0 : 11;
}
static int runProbe() {
    auto bounds=StackBounds::currentThreadStackBoundsInternal();
    probe_log("main.origin",reinterpret_cast<uintptr_t>(bounds.origin));
    probe_log("main.bound",reinterpret_cast<uintptr_t>(bounds.bound));
    if (!within(bounds) || !grow(4,bounds)) return 10;
    DWORD id=0, result=99;
    HANDLE thread=CreateThread(nullptr,0,worker,nullptr,0,&id);
    if (!thread) return 12;
    DWORD wait=WaitForSingleObject(thread,10000);
    bool read=wait==WAIT_OBJECT_0 && GetExitCodeThread(thread,&result);
    CloseHandle(thread);
    return read ? result : 13;
}
int main() { probe_open("C:\\STACKDIAG.LOG"); return probe_finish(runProbe()); }
#else
static MEMORY_BASIC_INFORMATION region(uintptr_t start, uintptr_t size, DWORD state, DWORD protect) {
    return {reinterpret_cast<void*>(start),reinterpret_cast<void*>(0x100000),size,state,protect,MEM_PRIVATE};
}
static void reset() {
    tib.StackBase=reinterpret_cast<void*>(0x110000); sp=0x10e800; pageSize=0x1000;
    queries=failQuery=modernCalls=0; shortQuery=nullTib=false;
    regions={region(0x100000,0xb000,MEM_RESERVE,0),region(0x10b000,0x1000,MEM_COMMIT,PAGE_READWRITE|PAGE_GUARD),region(0x10c000,0x4000,MEM_COMMIT,PAGE_READWRITE)};
}
static unsigned passes;
static void expect(bool valid) {
    try {
        auto bounds=StackBounds::currentThreadStackBoundsInternal();
        assert(valid); assert(reinterpret_cast<uintptr_t>(bounds.origin)==0x110000);
        assert(reinterpret_cast<uintptr_t>(bounds.bound)==0x101000);
    } catch (Rejected&) { assert(!valid); }
    ++passes;
}
int main() {
#if _WIN32_WINNT < 0x0602
    reset(); expect(true);
    reset(); regions={region(0x100000,0x10000,MEM_COMMIT,PAGE_READWRITE)}; expect(true);
    reset(); regions[1].Protect=PAGE_READWRITE; expect(true); // consumed guard
    reset(); regions[2].RegionSize=0x2000; regions.push_back(region(0x10e000,0x2000,MEM_COMMIT,PAGE_EXECUTE_READWRITE)); expect(true);
    reset(); regions[0]=region(0x100000,0x1000,MEM_COMMIT,PAGE_NOACCESS); regions.insert(regions.begin()+1,region(0x101000,0xa000,MEM_RESERVE,0)); expect(true);
    reset(); failQuery=1; expect(false);
    reset(); failQuery=2; shortQuery=true; expect(false);
    reset(); regions[1].AllocationBase=reinterpret_cast<void*>(0x101000); expect(false);
    reset(); regions[1].State=MEM_FREE; expect(false);
    reset(); regions[1].RegionSize=0; expect(false);
    reset(); regions[1].RegionSize=UINTPTR_MAX; expect(false);
    reset(); regions[1].Protect=PAGE_NOACCESS; expect(false);
    reset(); regions[2].Protect|=PAGE_GUARD; expect(false);
    reset(); regions[2].State=MEM_RESERVE; expect(false);
    reset(); regions[2].Protect=PAGE_READONLY; expect(false);
    reset(); tib.StackBase=reinterpret_cast<void*>(sp); expect(false);
    reset(); tib.StackBase=reinterpret_cast<void*>(0x111000); expect(false);
    reset(); pageSize=0; expect(false);
    reset(); pageSize=3000; expect(false);
    reset(); nullTib=true; expect(false);
    reset(); regions[2].AllocationBase=nullptr; expect(false);
    reset(); regions[2].Type=0; expect(false);
    reset(); sp=0x100800; regions={region(0x100000,0x10000,MEM_COMMIT,PAGE_READWRITE)}; expect(false);
    reset(); regions.clear(); tib.StackBase=reinterpret_cast<void*>(0x2002000); sp=0x2001800;
    for (uintptr_t start=0x100000; start<0x2002000; start+=0x1000) regions.push_back(region(start,0x1000,MEM_COMMIT,PAGE_READWRITE));
    expect(false); // bounded metadata walk
    printf("Legacy stack bounds: %u metadata/guard/failure cases passed\n",passes);
#else
    reset(); expect(true); assert(modernCalls==1 && !queries);
    puts("Modern stack bounds: OS API retained");
#endif
}
#endif
