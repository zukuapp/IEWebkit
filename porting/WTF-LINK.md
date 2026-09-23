# WTF archive and Windows SDK dependency checkpoint

Upstream: WebKitGTK 2.54.0, commit
`5220e80b97a253c60ed899361654142ab5021998`.

## Actual failures and fix

The complete MinGW x86 WTF static target now builds with the pinned ME source
patches. Continuation used one compiler worker and bounded 180/240-second
batches. No additional WTF compiler error appeared after the PathWalker include
fix; the final batch created `lib/libWTF.a`.

A small caller of the **actual archive's** `WTF::RandomDevice` then exposed:

1. `cannot find -lDbgHelp`: a case-sensitive MinGW SDK contains `libdbghelp.a`.
2. After correcting that spelling, the linked executable imported `Sleep` from
   `api-ms-win-core-synch-l1-2-0.dll`. That DLL is absent from the pinned ME
   baseline. The unconditional SDK `synchronization` library introduced it;
   a successful archive build had not detected this loader dependency.

The patches to `Source/WTF/wtf/PlatformJSCOnly.cmake` and `PlatformWin.cmake`
use canonical `dbghelp` spelling and remove `synchronization` **only when the
existing explicit `IEWEBKIT_WIN9X` option is enabled**. Kernel32 supplies Sleep
for this profile. Other profiles retain their dependency list and ordering;
no API-set replacement DLL, OS-version spoofing or weak RNG fallback is added.
If an unsupported synchronization API remains elsewhere in the full engine,
its unresolved link/import is still a gate, not silently supplied by this fix.

## Reproduce and distinguish the evidence

```
python3 porting/apply-webkit-patches.py --source "$IEWK_SOURCE"
ninja -C "$IEWK_BUILD" -j1 WTF
python3 porting/build-wtf-link-probe.py \
  --source "$IEWK_SOURCE" --build "$IEWK_BUILD" --output "$IEWK_PROBES"
python3 -m unittest discover -s porting/tests -v
```

All directories must be external to this repository. The builder:

- Verifies the reviewed CMake source hashes.
- Reuses the actual RandomDevice target's compiler configuration for a caller.
- Runs the actual upstream CMake platform lists, using Win9x enabled/disabled
  for both JSCOnly and Windows dependency lists.
- Links against `libWTF.a` with static compiler runtime settings and records
  commands, selected libraries and archive digest in `provenance.json`.

All four links passed. Nine existing host patch/bootstrap regressions pass.
These are host-side CMake/compiler/link checks; no Linux-host copy of the Windows
RandomDevice implementation was executed. Both Win9x executables pass the pinned
ME import audit. Both modern-library controls retain the API-set Sleep import
and correctly fail that same ME audit; this is an expected negative control,
not a failure of the modern Windows profile. The caller's compiler declarations
remain ME in all four cases; the controls test dependency-list preservation,
not a complete modern engine build.

Run the static audit separately for each Win9x executable:

```
python3 porting/audit-pe-imports.py \
  --binary "$IEWK_PROBES/PlatformJSCOnly-win9x-wtf-random.exe" \
  --dll-dir "$IEWK_ME_DLLS" --baseline "$IEWK_ME_BASELINE" \
  --output "$IEWK_PROBES/jsconly-imports.json"
```

Repeat with `PlatformWin-win9x-wtf-random.exe`. The probe invokes a real WTF
method, but linker archive selection and section collection include only its
reachable code. This does **not** import-audit all of WTF, bmalloc, ICU or JSC.
The archive is the JSCOnly WTF build in both cases; testing the Windows dependency
list does not constitute a WebCore/Windows engine build.

## Recorded artifacts and remaining gates

External area: `/srv/zuku/deploy-work/20260924-wtf-link`.
Earlier compile logs: `/srv/zuku/deploy-work/20260924-language/wtf-continuation{2,3,4}.log`.

- Full WTF archive: `902e0912814f4aaac98e00e2290478d2fbb6b2e71cc25075dc2e72bd28f0551f`.
- First linked probe with unwanted API-set import:
  `2e743cd9ec417764743008d21870d10af894da456db094226d6e3ab4596e1d81`.
- Corrected Win9x caller, both platform lists:
  `347ec3e8012799c8b0e7cc2a72f13c9be3d9c864161a9f4f84f2d2b9c28f2eda`.
- ME baseline manifest:
  `38cdb3eaadce250b5ec96234a92a14c6a258920e856bd94fc95c5e70ce82d7c9`.

`link-before.log` reproduces the library-name failure; `repro/jsconly-imports.json`
records the first loader gap. `r2/provenance.json`, the four link logs and import
reports record the corrected and negative-control results. PE timestamps may
change the executable hashes when rebuilt.

No guest was used. The actual RandomDevice calls (including its current Unicode
provider acquisition), entropy quality and concurrency still require ME runtime
validation. Export presence does not establish that a Win9x API is implemented.

No `jsc.exe`, JavaScriptCore archive or WebCore renderer has been completed here.
The configured JSC shell independently requested mixed-case `Winmm` in
`Source/JavaScriptCore/shell/CMakeLists.txt`; adding that exact shell dependency
to the otherwise successful probe reproduced `cannot find -lWinmm` in
`r2/shell-library-negative.log`. The later isolated shell-library fix and
positive link probe are recorded in [JSC-SHELL-LINK.md](JSC-SHELL-LINK.md).
The existing build also still references the original ICU prefix; a final ME
link must use the separately verified Win9x ICU prefix. Full engine linking,
complete import closure and actual IE/ME execution remain required.
