# Compiler-specific Windows thread naming

The actual ME/MinGW `ThreadingWin.cpp` compile failed on `__except`. That SEH
block only reports a thread name to an attached Visual Studio debugger; it is
not an engine exception handler or synchronization primitive. Microsoft explains
this mechanism in [thread debugging tips](https://learn.microsoft.com/en-us/visualstudio/debugger/tips-for-debugging-threads?view=visualstudio).

The source-hash-pinned patch keeps the existing record, exception number and
SEH body under `COMPILER(MSVC)`, including compatible MSVC-mode Clang. MinGW
uses `OutputDebugStringA` on the existing normalized name and then performs the
same mandatory `initializeCurrentThreadEvenIfNonWTFCreated` call. It never
raises the synthetic naming exception without a matching SEH handler and does
not pretend C++ `catch` handles Windows SEH.

The fallback is **diagnostic output, not a debugger thread label**. No
`SetThreadDescription` dependency is added; registration of thread names in
newer OS tools remains separate work. Microsoft's
[OutputDebugStringA contract](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-outputdebugstringa)
describes output to the debugger and no action when no debugger is active.

## Reproduce and measured results

```
python3 porting/apply-webkit-patches.py --source /external/webkitgtk-2.54.0
python3 porting/build-thread-name-probes.py \
    --source /external/webkitgtk-2.54.0 --output /external/name-probes
```

The builder extracts the exact Windows name section only after matching its
patched source hash. Three host cases check that normalization happens first,
its returned value is sent to the output API, and mandatory initialization
happens exactly once afterward. Normalization itself is a stub here; this does
not certify the independent normalization algorithm. Clang targeting actual
MSVC x86 and x64 compiles the preserved SEH section with real `__try`/`__except`
syntax, not macros that fake exception support. Those syntax objects are not
linked to an MSVC CRT or executed. Nine patch/bootstrap unit tests pass.

A full actual `ThreadingWin.cpp` object compiled using the existing MinGW build
command with Win7 API declarations (`WINVER/_WIN32_WINNT=0x0601`,
`NTDDI_VERSION=0x06010000`, no `_WIN32_WINDOWS`) and an external object/dependency
output. Exact compiler arguments are retained in external `modern-command.json`.
The resulting object SHA-256 is
`7fdb24da432436d8bb1274e20d2fdb04c75e142982354121585695067b2726ca`.
This is a compile gate, not a Win7 runtime certification. The actual ME object
retry now fails only at the SRW lock and condition-variable calls; its complete
object is still not built.

The static ME naming fixture passes the pinned import audit with zero gaps;
it imports `OutputDebugStringA` and no `RaiseException`. SHA-256:
`9f509c809129f63a8771779eaab8717b32c157e4a71c4b2e21ef6ce34fa58078`.
It writes `C:\NAMEDIAG.LOG`; it has **not run in a guest**, and no debugger
attachment or full WTF thread lifetime has been tested.
External evidence: `/srv/zuku/deploy-work/20260924-thread-name/`.
