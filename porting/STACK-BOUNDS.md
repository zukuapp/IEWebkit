# Pre-Windows8 stack bounds checkpoint

The pinned `StackBounds.cpp` failed under ME declarations at
`GetCurrentThreadStackLimits`, documented for
[Windows8 and later](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadstacklimits).
The new hash-pinned branch selects `_WIN32_WINNT < 0x0602`; Windows8+ keeps the
original OS API. This is source/compile evidence, not an ME or NT5 runtime pass.

## Boundary derivation

The earlier branch reads only the common `NT_TIB::StackBase` through the
compiler's inline `NtCurrentTeb`, and uses `VirtualQuery` on the current stack
pointer to identify the allocation's low address. It does not treat the moving
TIB committed-stack limit as the reservation limit. The linked x86 fixture
emits `mov fs:0x18` directly; it imports no `NtCurrentTeb` function.
Microsoft's current [NtCurrentTeb page](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-ntcurrentteb)
does not certify ME. **FS/TIB layout and behavior on actual ME and NT5 remain an
explicit guest gate**, even though the toolchain can generate these instructions.

A metadata walk validates the complete private allocation up to the stack top:
page alignment, same allocation, nonzero forward progress, bounded arithmetic,
reserve/commit states and writable committed regions. Active stack pages that
may be read by GC must be committed and unguarded. A bottom inaccessible page
is permitted only inside the excluded page. No memory page is touched or
committed by the walk. It supports more than three regions, fully committed
stacks and a consumed guard; it does not infer a fixed reserve/guard/commit layout.
These checks follow the region contract in
[VirtualQuery](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualquery).

One bottom page is excluded conservatively, in addition to WTF's existing
recursion reserve. The stack allocation and guard relationship is described in
[Thread Stack Size](https://learn.microsoft.com/en-us/windows/win32/procthread/thread-stack-size).
Inconsistent metadata, an unsupported switched/custom stack, or more than 4096
regions causes a release assertion. The code does not fabricate bounds or
silently enlarge them. It cannot protect against another component concurrently
changing stack protection or deallocating memory. Full collector/recursion and
fiber integration still require actual engine tests.

## Reproduce

```
python3 porting/apply-webkit-patches.py --source /external/webkitgtk-2.54.0
python3 porting/build-stack-bounds-probes.py \
    --source /external/webkitgtk-2.54.0 --output /external/stack-probes
ninja -C /external/build-me-jsc -j1 \
    Source/WTF/wtf/CMakeFiles/WTF.dir/StackBounds.cpp.obj
```

The exact patched Windows section is extracted after its source hash is checked.
Host fixtures mock only TIB/Win32 queries. ME, NT5 and Windows7 declarations each
pass 24 layout/failure cases; the Windows8 branch retains its API (73 checks).
The original source fails the rejection regression. Nine patch/bootstrap unit
tests also pass. The actual WTF object now compiles, SHA-256
`318437442c585c254a44192b6a350f77116c4b638906bbf2f4daefe20a10b523`.
Its undefined symbols are only `GetSystemInfo`, `VirtualQuery` and `abort`.

The static PentiumIII x86 fixture passes the pinned ME import audit with no
gaps, SHA-256
`bf03484dd8f6ac17a2faf07545251ecb3918e4ebbe4cf753760eabd29f9898df`.
It has **not executed in a guest**. Its future runtime checks cover the main
thread, a worker with a non-null thread-ID output, and five nested 8 KiB frames;
it writes `C:\STACKDIAG.LOG`. Exit 50 means a boundary release assertion,
10/11 means main/worker containment or stable bounds failed, and 12/13 means
thread creation or completion failed. No full engine readiness is claimed.

External logs: `/srv/zuku/deploy-work/20260924-stack-bounds/`.
The next focused compile, `win/ThreadingWin.cpp`, initially failed at MinGW/pas_thread pthread type collisions, MSVC SEH syntax
and missing SRW/condition-variable APIs. The subsequent
[process-header checkpoint](PROCESS-HEADER.md) removes the typedef collisions;
SEH and synchronization remain. It needs a separate thread backend and
actual contention/wakeup/timeout tests; this stack patch does not implement it.
