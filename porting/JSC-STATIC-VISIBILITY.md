# Static JavaScriptCore generator and interpreter compile checkpoint

The source is pinned to WebKitGTK 2.54.0, WebKit commit
`5220e80b97a253c60ed899361654142ab5021998`, archive SHA-256
`846fd19ccedbae1dbfe904f26dbf2d68a800a33a50caf2ad5222c8dcb3f25682`.
The reviewed patch applies only to `Source/JavaScriptCore/CMakeLists.txt` with
the explicit `IEWEBKIT_WIN9X AND ENABLE_STATIC_JSC` profile. The upstream file
SHA-256 is `60ea0825f56f7e8e7634ca3536935d9a157de46f3b0d1a6354dceb70a3373b25`;
the patched file SHA-256 is
`3363b9cf78f1ed870a7e406228a351a6d794500303fe02a0c3c1cce59130f289`.

## Reproduction and cause

The real x86 MinGW `jsc` target stopped while compiling
`LLIntOffsetsExtractor.cpp` with two errors: inline JSC destructors in
`IsoSubspace.h` and `VM.h` were marked `dllimport`. Its compiler command did
not define `STATICALLY_LINKED_WITH_JavaScriptCore`, although the selected JSC
profile builds a static library and the final `jsc` shell has that definition.
The offset extractor is a separate executable target, so it does not inherit
the shell's compile definitions. Adding exactly that definition to its actual
compiler command compiled and linked the extractor in an isolated hypothesis
test. The next full build then reproduced the same two errors in the separate
`LowLevelInterpreterLib` object target; the same isolated definition compiled
its real `LowLevelInterpreter.cpp` object.

The patch gives only these two static JSC targets the missing definition. It
does not change the shared JSC profile or source API declarations. Repeat with
the pinned external source and existing x86 build directory:

```
python3 porting/apply-webkit-patches.py --source "$IEWK_SOURCE"
ninja -C "$IEWK_BUILD" -j1 bin/LLIntOffsetsExtractor.exe
ninja -C "$IEWK_BUILD" -j1 LowLevelInterpreterLib
python3 -m unittest discover -s porting/tests -v
```

The first target compiled and linked. The second target compiled its complete
object. Nine patch/bootstrap regression tests passed. This is a source and
host cross-build checkpoint, not a Windows ME runtime result.

## Import audit and next build gate

The linked `LLIntOffsetsExtractor.exe` SHA-256 is
`4a72b71ca874e976c0732bc412bb561c720534ad18244d77d4694b0efe24e59d`.
Its pinned Windows ME export audit **fails** because it imports
`libgcc_s_dw2-1.dll`, which is absent from the pinned OS baseline. The earlier
`LLIntSettingsExtractor.exe` has the same import gap. These are build-time
offset tools; they have not run on ME or been included in a guest client. The
baseline SHA-256 is `38cdb3eaadce250b5ec96234a92a14c6a258920e856bd94fc95c5e70ce82d7c9`.

Continuing the actual `jsc` build reached
`JavaScriptCore/DerivedSources/ArrayConstructor.lut.h` and then failed because
the host Perl 5.40 installation lacks `bigint.pm`, required by upstream
`create_hash_table`. `perl -Mbigint` reproduces the host prerequisite failure.
The current build also points at the older ICU x86 prefix; a final ME link must
use the separately tested Win9x ICU prefix and audit the full linked binary and
all shipped dependencies. No `jsc.exe`, DOM/layout renderer, or ME execution
pass is claimed.

Evidence: `/srv/zuku/deploy-work/20260924-jsc-next/`. `build-before.log`
records the first error, `build-after-jsc.log` records the second, and
`build-after-both.log` records the Perl gate. `offsets-imports.json` and
`settings-imports.json` contain the import reports. All build outputs remain
outside this repository. The evidence manifest SHA-256 is
`cc33daec3d50a4054318d9c4327f91bafd98dc65c62b0701fd8dabb75bc6705c`.
