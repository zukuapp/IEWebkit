# Development host matrix verification

```sh
python3 -m unittest discover -s test -p 'test_*.py'
python3 tools/build_host_matrix.py --jobs 2 --work-dir /external/new-host-build
python3 tools/verify_host_pe.py /external/new-host-build/ie55-winme-x86-classic-host-v0.1.0.0
```

The current manifest has **17 IE/OS profiles and 55 architecture/security-mode
combinations**. The matrix expands the manifest instead of selecting a
representative architecture or silently dropping Protected Mode variants.
`--profile ie11-win81` builds all six combinations for that profile. Use a new
external directory for each run; existing artifacts are never overwritten.
Concurrency is bounded to 1–4 compiler jobs (default 2).

All targets in one run use the same captured host source bytes. Each build keeps
its source digests, compiler identities, stable command description, DLL digest
and linked-binary verification. `matrix-all.json` records every result, hash,
elapsed time and the overall source snapshot. PE metadata validation reads the
DLL directly, not just the generated RC file. It checks machine/optional-header
architecture, DLL flags, zero PE timestamp, requested OS/subsystem versions,
fixed file/product versions, prerelease flags, file OS/type, translation, and
every variant string. Negative tests corrupt real x86/x64 linked fixtures to
check rejection of mismatched architecture, metadata, truncation and resources.

The reader follows Microsoft's [PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
and [VS_VERSIONINFO layout](https://learn.microsoft.com/en-us/windows/win32/menurc/vs-versioninfo).
It is a bounded build verifier, not a general-purpose PE loader. A zero timestamp
does not promise identical bytes from different toolchain versions or paths.

CI builds every combination and retains metadata/reports for 14 days. It does
not publish installers or engine binaries. Every result is explicitly
`engine_included=false`, `guest_verified=false`, `release_eligible=false`.
Correct VERSIONINFO labels do not demonstrate Protected Mode implementation,
OS import compatibility, DLL loading in a guest, or WebKit rendering. Those
remain separate target-specific gates.
