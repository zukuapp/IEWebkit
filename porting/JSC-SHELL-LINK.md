# JSC shell WinMM link checkpoint

Upstream WebKitGTK 2.54.0 commit
`5220e80b97a253c60ed899361654142ab5021998` lists `Winmm` in
`Source/JavaScriptCore/shell/CMakeLists.txt`. The case-sensitive MinGW cross
linker has `libwinmm.a`. A CMake script loaded the **actual upstream shell
list** with only unrelated target declarations stubbed; it returned `Winmm`.
Linking a tiny `timeGetTime()` caller with that list failed with
`cannot find -lWinmm`.

The SHA-pinned patch `webkit-2.54.0-jsc-shell-winmm-case.patch` changes only
the import-library spelling to `winmm`. Repeating the same CMake extraction
returned `winmm`; the same caller linked. Its PE SHA-256 is
`486c0f8eeff02334dfb0ac2e58fb616429e7c4330cf8e053fab15539f838bfd2`.
The checksum-pinned Windows ME DLL export audit returned zero missing imports.
The patch manager verified all 16 source/patch hashes. Logs, CMake extraction,
source and import report are in
`/srv/zuku/deploy-work/20260924-jsc-shell-link/`.

This fixes the JSC **shell dependency spelling**, not the JSC engine. `jsc.exe`
has not been built or run. The next gates are the actual JavaScriptCore target
compile/link, a Win9x ICU prefix at final link, a complete dependency/import
audit, and execution inside the ME guest. The linked WinMM caller has not run
in ME; its import audit is only a loader preflight.
