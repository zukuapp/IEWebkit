# Process-query header isolation

The actual MinGW `ThreadingWin.cpp` compile failed because `pas_process.h`
included `pas_utils.h`, which included `pas_thread.h`. That Windows compatibility
header defines pthread types/macros that collide with the CRT pthread declarations
already included by C++ `<memory>`/`<mutex>`. Both include orders failed.

The process API declares only `bool pas_process_is_shutting_down(void)`. Its
header now includes `pas_config_prefix.h`, `pas_utils_prefix.h` and `stdbool.h`,
using the same underlying linkage/visibility macros directly. The configuration
prefix is retained because the utility prefix contains architecture-dependent
inline helpers. `pas_process.c` explicitly includes `pas_platform.h` for its
OS selection. Neither libpas's pthread implementation nor the CRT thread ABI is
changed. This fixes this public include path; direct inclusion of the Windows
`pas_thread.h` beside the CRT pthread header is still a separate integration risk.

## Validation

```
python3 porting/apply-webkit-patches.py --source /external/webkitgtk-2.54.0
python3 porting/build-process-header-probes.py \
    --source /external/webkitgtk-2.54.0 --output /external/process-probes
ninja -C /external/build-me-jsc -j1 \
    Source/bmalloc/CMakeFiles/bmalloc.dir/libpas/src/libpas/pas_process.c.obj
```

The probe checks exact patched hashes, compiles the actual C implementation,
and links a C++ caller including the header both before and after CRT headers.
Linux, ME-declaration MinGW and modern-declaration MinGW builds each test both
hidden/default visibility settings: 12 C/C++ links pass. Four Linux executions
confirm the existing non-Windows false result; two ELF symbol checks verify
visibility. `-Werror=undef` checks that architecture macros remain available.
The fixture also rejects a leaked `pthread_t` macro and verifies the API type.
The original header failed both MinGW include orders; the actual bmalloc C
object now compiles. No Windows execution was performed.

The selected static ME-declaration executable passes the pinned ME import audit,
SHA-256 `7f092b1f10125678c5ad577f330b8eb4a5186f813370229c926cada79aa762dc`.
An import pass does not establish shutdown-time or dynamically resolved API
behavior. The existing `GetModuleHandleW`/`RtlDllShutdownInProgress` discovery
logic is unchanged and still needs actual target lifetime testing.

The real `ThreadingWin.cpp` retry no longer reports pthread typedef collisions
or a redefined `PTHREAD_ONCE_INIT`. It still fails at `__except` and the absent
SRW/condition-variable calls. The full object and JSC engine remain unbuilt.
External before/after and link logs:
`/srv/zuku/deploy-work/20260924-process-header/`.
