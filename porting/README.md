# IEWebkit engine port bootstrap

IEWebkit's target is a general WebKit + JavaScriptCore document engine hosted
inside Internet Explorer, including a Windows ME x86 variant and later IE/OS
variants. This directory prepares the modern engine source and measures the
first porting gaps. It does not contain a working WebKit DLL or claim a browser
or operating-system pass. The ZUKU GDI screen adapter is a separate client and
does not satisfy the full-engine objective.

## Pinned upstream

- Official source release: WebKitGTK **2.54.0**, published 2026-09-16.
- WebKit Git tag: `webkitgtk-2.54.0`.
- Corresponding source commit: `5220e80b97a253c60ed899361654142ab5021998`.
- Archive SHA-256: `846fd19ccedbae1dbfe904f26dbf2d68a800a33a50caf2ad5222c8dcb3f25682`.
- The checksum was compared with the [official release sums](https://webkitgtk.org/releases/webkitgtk-2.54.0.tar.xz.sums).
- The GTK source archive omits Windows platform implementation files. The
  bootstrap supplements **30** precisely pinned Windows WTF/JSC/CMake files
  from the same commit. Every supplement has its own SHA-256 in
  `webkit-source.json`; it is not fetched from a moving branch.

The source includes BSD-style and LGPL components. Preserve upstream per-file
notices, `Source/JavaScriptCore/COPYING.LIB`, and WebCore's `LICENSE-APPLE`,
`LICENSE-LGPL-2`, and `LICENSE-LGPL-2.1`. A distributed modified engine needs
its corresponding source/patches and notices. Build artifacts and upstream
archives belong in an external build directory, not this repository.

## Reproduce the first build stage

Prerequisites: Python 3.12+, CMake, Ninja, i686 MinGW GCC/G++, Perl with English,
FindBin and JSON::PP, Ruby 2.5+, and a target x86 ICU 70.1+ build (data/i18n/uc).
The observed compiler was i686-w64-mingw32 GCC/G++ 15.1.1.

```
python3 porting/bootstrap.py --work-dir /absolute/external/build-area
sh porting/configure-jsc-me.sh \
    /absolute/external/build-area/webkitgtk-2.54.0 \
    /absolute/external/build-area/build-me-jsc
cmake --build /absolute/external/build-area/build-me-jsc --target jsc --parallel 2
```

The bootstrap does not overwrite an existing unrecognized source tree. A
successful tree has a pin stamp and can subsequently carry deliberate patches.
The configure profile is an **interpreter bootstrap**, with JIT, remote inspector,
and WebAssembly disabled and bundled mimalloc selected. It is not a final product
feature profile. Upstream explicitly marks `ENABLE_WEBASSEMBLY` and `ENABLE_C_LOOP`
as conflicting options (`Source/cmake/WebKitFeatures.cmake`), so modern WASM on
ME still requires a separate implementation decision and validation. Disabling
WASM for this first compile must not become an unreported product limitation.

## Measured result

On 2026-09-23, checksum-pinned extraction and all supplemental downloads passed.
The MinGW32 compiler's C/C++ ABI checks passed. The first configuration stopped
at missing target ICU. The pinned ICU 78.3 x86 compile bootstrap now builds and
installs successfully, and JSC configuration completes with those target archives.
Host Perl/Ruby build prerequisite checks pass. The actual `jsc` build now links
`libbmalloc.a` and compiles initial WTF files. The next observed blocker,
`DateMath.cpp` at `GetTimeZoneInformationForYear`, now passes a focused object
compile after the standard-timezone patch. A later focused continuation compiled
six more generic WTF objects and the patched Windows file-handle object. The
next explicitly tested object, `StackBounds.cpp`, now compiles after a validated
legacy metadata fallback. `win/ThreadingWin.cpp` and generic `Threading.cpp` now compile with ME
declarations after isolating pthread headers, debugger-naming SEH and adding
a pre-Vista synchronization backend. Host contention/wakeup tests pass; actual
event/scheduler/TLS behavior remains a guest gate. See [LEGACY-SYNC.md](LEGACY-SYNC.md). No `jsc.exe` or engine was produced. See
[DATE-OFFSET.md](DATE-OFFSET.md), [FILE-TRUNCATE.md](FILE-TRUNCATE.md),
[STACK-BOUNDS.md](STACK-BOUNDS.md) and the
[17-profile dependency audit](VARIANT-DEPENDENCIES.md).

The ICU dependency bootstrap is now available in `build-icu-x86.sh`; its
unmodified upstream profile uses Windows 7 declarations and must not be treated
as a Windows ME runtime port. A separate opt-in Win9x dependency profile now
builds; its expanded native probe passes the ME import audit and the actual
Korean ME guest smoke (Unicode, collation, locale and timezone). See [ICU.md](ICU.md) for reproducible commands,
measured compile gates and the linked-binary audit. Do not point the target
linker at host Linux ICU or use a newer-Windows binary as proof of ME compatibility.

## Reviewed source patches

`configure-jsc-me.sh` applies the source-hash-pinned patches from
`webkit-patches.json` before configuration. `apply-webkit-patches.py` verifies
original and final hashes, applies each patch atomically, supports an already
applied patch, and rejects unexpected edits or symlink targets. Its tests cover
those preservation boundaries.

- `webkit-2.54.0-mingw-size-t.patch`: converts Win32 `SIZE_T` to `size_t` before
  `std::min`; MinGW32 uses distinct unsigned types of the same width.
- `webkit-2.54.0-win9x-declarations.patch`: adds opt-in `IEWEBKIT_WIN9X` with
  coherent Windows header declarations, preserving upstream defaults otherwise.
  This prevents Win10 `NTDDI_VERSION` from conflicting with the ME target macros.

- `webkit-2.54.0-gcc-x86-pair.patch`: packs two 32-bit pointers into a 64-bit
  integer on GCC x86, with pointer-width shifts. The 64-bit GCC representation
  remains 128 bits, and compiler atomic operations are retained.
- `webkit-2.54.0-win9x-mimalloc-lock.patch`: adds an atomic owner/recursion lock
  for explicit Win9x declarations. Try-lock uses one acquire/release CAS, unlock
  publishes a release store, and blocking contention sleeps for one millisecond.
  Other Windows builds retain SRW locks. It allocates no handles or heap memory;
  it does not provide FIFO fairness, priority inheritance or abandoned-owner
  recovery. Export presence does not prove runtime semantics.
- `webkit-2.54.0-portable-tick-literal.patch`: replaces the MSVC `I64` literal
  suffix with standard `LL`, preserving the value used for 32-bit tick wrap.
- `webkit-2.54.0-win9x-standard-timezone.patch`: uses current Win9x timezone
  settings for the standard offset and corrects Boolean/local-year semantics
  in the modern Windows path. Seasonal and historical rules remain separate.

The initial system-malloc experiment was rejected by WebKit's Windows allocator
contract; the bootstrap selects bundled mimalloc instead. These patches do not
complete the Win9x OS abstraction.

- `webkit-2.54.0-pre-vista-truncate.patch`: preserves file cursor and primary
  failure errors around a pre-Vista `SetEndOfFile` fallback. The full object
  compiles; ME/NT5 boundary probes pass, actual filesystem guest test is pending.

- `webkit-2.54.0-legacy-stack-bounds.patch`: validates common TIB/VirtualQuery
  allocation metadata on pre-Windows8 builds, preserving the modern API. Exact
  host fixtures and actual object compile pass; FS/TIB guest behavior is pending.

- `webkit-2.54.0-process-header.patch` and `process-platform.patch`: isolate the
  process query declaration from libpas pthread aliases, retaining C linkage,
  architecture configuration and visibility. See [PROCESS-HEADER.md](PROCESS-HEADER.md).

- `webkit-2.54.0-threading-win.patch`: preserves MSVC debugger naming and
  uses a diagnostic-only fallback on MinGW, without skipping required thread
  initialization. See [THREAD-NAME.md](THREAD-NAME.md) for compiler/runtime gates.

- The cumulative Windows-thread patch also adds the pre-Vista per-waiter event
  condition backend, with `webkit-2.54.0-legacy-sync-types.patch` for its constexpr
  atomic mutex/queue types. Vista+ retains the existing SRW ABI.

- `webkit-2.54.0-sdk-header-case.patch`: uses canonical `windows.h` spelling
  for the PathWalker SDK include on case-sensitive cross-build hosts. Both
  filesystem objects compile; direct import-surface audit passes, with Unicode
  filesystem behavior still a guest gate. See [SDK-HEADER-CASE.md](SDK-HEADER-CASE.md).

## Allocator probes

Build tests against the actual patched headers:

```
sh porting/build-allocator-probes.sh \
    /absolute/external/build-area/webkitgtk-2.54.0 \
    /absolute/external/build-area/allocator-probes
```

The native host pair test passed packing, failed CAS, concurrent CAS/load/store
and final counter checks. The Pentium III x86 fixture emits `cmpxchg8b` without a
libatomic dependency. Both ME-target fixtures pass the pinned ME import audit.
The modern-declaration lock regression build retains its SRW imports. The build
script does **not** execute Windows binaries or certify the ME guest.

`pas-pair-smoke.exe` checks pair layout, alignment and concurrent updates.
`mimalloc-lock-smoke.exe` checks try-lock failure from another thread, blocking
handoff, 100,000 contended increments and teardown. Guest evidence must include
the exact executable digest, exit code and output; exported ME API stubs can
still fail these behavioral tests.

The first actual ME run failed: the pair probe exited 4 at thread creation and
the lock probe exited 1 at its initial try-lock. The revised probes supply
non-null `CreateThread` thread-ID outputs, as required by Win9x. They write
native file telemetry to `C:\PAIRDIAG.LOG` and `C:\LOCKDIAG.LOG`, independently
of console redirection. This fixture correction does not resolve or excuse the
separate initial try-lock failure; its immediate API error is recorded for
diagnosis. In the actual ME rerun, the revised pair probe exited 0 and the lock
probe still exited 1. Native telemetry confirmed `TryEnterCriticalSection`
returns error 120 (`ERROR_CALL_NOT_IMPLEMENTED`) on the tested ME guest. The
Critical Section branch was therefore replaced by the Win9x atomic owner lock
described above. Its revised guest probe adds recursive try/blocking acquisition
and verifies that partial recursive release cannot hand ownership to another
thread. Both revised probes passed in the actual standard-VGA ME guest; see
[the allocator guest record](ALLOCATOR-GUEST.md). This verifies the tested lock
contract, not full mimalloc allocation behavior or JSC execution.


## Concrete OS abstraction seams

The [current Windows port documentation](https://docs.webkit.org/Ports/WindowsPort.html)
limits its supported target to 64-bit Windows. Pinned source provides more precise
porting evidence:

| Source | Actual dependency / required work |
| --- | --- |
| `Source/cmake/OptionsJSCOnly.cmake` | Forces `_WIN32_WINNT=0x0A00` / Windows 10 declarations; needs a genuine Win9x platform selection, not just an extra compiler flag. |
| `Source/cmake/OptionsCommon.cmake` | C++23; compiler can target x86 but its CRT and thread-local destruction support must also run on ME. |
| `WTF/wtf/ThreadingPrimitives.h`, `win/ThreadingWin.cpp` | Pre-Vista mutex/condition objects compile and pass host race tests; actual ME event/scheduler behavior and C++ `thread_local` cleanup remain unverified. |
| `WTF/wtf/StackBounds.cpp` | Pre-Windows8 metadata fallback compiles and passes host cases. Main/worker growth, FS/TIB and collector behavior remain actual guest gates. |
| `WTF/wtf/win/FileHandleWin.cpp` | Pre-Vista truncation fallback now compiles and passes boundary tests. Unicode/file-path and actual filesystem behavior remain guest gates. |
| `WTF/wtf/DateMath.cpp` | Standard-offset patch passes isolated compilation and host regression probes. Actual ME standard-offset query passes; separate seasonal conversion remains unverified. See `DATE-OFFSET.md`. |
| `WTF/wtf/CurrentTime.cpp` | Already uses `GetTickCount()` on i386; `GetTickCount64` is **not** an x86 gap. Keep the existing QPC sanity checks. |
| `WTF/wtf/PlatformJSCOnly.cmake` | Links `synchronization`, DbgHelp and other Windows libraries; a Win9x port must remove/replace unavailable services. |
| `Source/cmake/OptionsWin.cmake` | Defaults to Skia in this pinned revision; a Cairo branch remains with `USE_SKIA=OFF`. Full WebCore additionally requires curl, HarfBuzz, ICU, JPEG, XML, OpenSSL, PNG, SQLite, zlib, PSL and WebP, plus a port of the Windows view/event layer. |

The Windows documentation's Cairo description is broader than this revision's
actual default. Build choices must follow the pinned source. A software Cairo
backend is a candidate for the legacy variant, but no WebCore build or rendering
quality claim has been established.

`required-api-probe.c` references an exact subset of unconditionally used modern
Windows APIs and compiles to a real x86 import fixture. It never executes those
calls. Audit it against a separately licensed, checksum-pinned ME KERNEL32:

```
python3 porting/audit-api-probe.py \
    --kernel32 /absolute/extracted-me/kernel32.dll \
    --kernel32-sha256 YOUR_VERIFIED_BASELINE_SHA256 \
    --output-dir /absolute/external/build-area/api-audit
```

Exit 1 means required exports are missing, not a successful engine build. The
observed ME baseline lacked eight fixture imports: `AcquireSRWLockExclusive`,
`ReleaseSRWLockExclusive`, `TryAcquireSRWLockExclusive`,
`SleepConditionVariableSRW`, `WakeConditionVariable`, `WakeAllConditionVariable`,
`GetCurrentThreadStackLimits`, and `SetFileInformationByHandle`. This is a lower
bound: a full linked engine and its dependency DLLs need their own import audit.
Even APIs that are exported by ME can have incomplete behavior, so import checks
cannot replace guest tests.

## Next integration order

1. Build/audit target ICU and the CRT; create a real `WIN9X` configuration that
   uses C_LOOP and the required target ABI without importing Windows 10 services.
2. Implement and stress-test WTF threads, conditions, stack bounds, allocation,
   files, clock and text support in the ME guest. Build and run `jsc` language/GC
   tests before embedding the engine.
3. Port WebCore software layout/paint, fonts, images and general validated TLS
   network loading. Connect its view/input/navigation interface to the IE host
   adapter; the engine must accept arbitrary permitted sites rather than a
   ZUKU-specific screen protocol.
4. Verify complete document navigation, redirects, DOM/JS, CSS, Korean input,
   history, forms, frames, TLS and teardown on each version's actual guest.
   Keep full-engine, JS/WASM and acceleration requirements explicit in the
   version compatibility matrix.
