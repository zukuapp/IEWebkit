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
and WebAssembly disabled and system malloc requested. It is not a final product
feature profile. Upstream explicitly marks `ENABLE_WEBASSEMBLY` and `ENABLE_C_LOOP`
as conflicting options (`Source/cmake/WebKitFeatures.cmake`), so modern WASM on
ME still requires a separate implementation decision and validation. Disabling
WASM for this first compile must not become an unreported product limitation.

## Measured result

On 2026-09-23, checksum-pinned extraction and all supplemental downloads passed.
The MinGW32 compiler's C/C++ ABI checks passed. Configuration then stopped at
missing **target ICU 70.1+** include/data/i18n/uc libraries. The configured package
repositories did not offer `mingw32-icu`. Host Perl/Ruby build prerequisites were
installed and their checks passed. No `jsc.exe` or engine was produced.

The next dependency build should cross-compile current ICU for x86 with its
own Win9x API/CRT import audit. Do not point the target linker at host Linux ICU
or use a newer-Windows ICU binary as proof of ME compatibility.

## Concrete OS abstraction seams

The [current Windows port documentation](https://docs.webkit.org/Ports/WindowsPort.html)
limits its supported target to 64-bit Windows. Pinned source provides more precise
porting evidence:

| Source | Actual dependency / required work |
| --- | --- |
| `Source/cmake/OptionsJSCOnly.cmake` | Forces `_WIN32_WINNT=0x0A00` / Windows 10 declarations; needs a genuine Win9x platform selection, not just an extra compiler flag. |
| `Source/cmake/OptionsCommon.cmake` | C++23; compiler can target x86 but its CRT and thread-local destruction support must also run on ME. |
| `WTF/wtf/ThreadingPrimitives.h`, `win/ThreadingWin.cpp` | SRW mutexes and condition variables, MSVC structured exception syntax, and C++ `thread_local` cleanup. Needs tested ME mutex/condition/thread backend. |
| `WTF/wtf/StackBounds.cpp` | Unconditional Windows `GetCurrentThreadStackLimits`; ME needs correct main/worker stack bounds including guard-page behavior. |
| `WTF/wtf/win/FileHandleWin.cpp` | `SetFileInformationByHandle`, modern seeking and Unicode/file paths; adapt using available ME APIs and preserve offsets/error behavior. |
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
