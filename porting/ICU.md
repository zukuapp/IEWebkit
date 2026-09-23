# ICU dependency bootstrap

This builds current ICU4C for an x86 **compile bootstrap**. ICU's unmodified
MinGW configuration selects Windows 7 APIs. Its output must not be installed
or advertised as a working Windows ME dependency.

The pinned version is ICU4C 78.3, tag `release-78.3`, commit
`21d1eb0f306e1141c10931e914dfc038c06121da`. Source SHA-256 is
`3a2e7a47604ba702f345878308e6fefeca612ee895cf4a5f222e7955fabfe0c0`.
The source was verified both against the official release asset digest and
[Unicode's SHA-512 manifest](https://github.com/unicode-org/icu/releases/download/release-78.3/SHASUM512.txt).
`icu-source.json` pins both. Preserve ICU's Unicode License V3 and third-party
notices when redistributing modified source or artifacts.

## Build

Use an external directory. Both bootstrap scripts reject source-repository
paths, including symlinks into the repository, before creating a directory or
making a network request. Build jobs are limited to 1 or 2 by the wrapper.

```
sh porting/build-icu-x86.sh /absolute/external/icu-build 2
sh porting/configure-jsc-me.sh \
    /absolute/external/webkit-build/webkitgtk-2.54.0 \
    /absolute/external/webkit-build/build-me-jsc \
    /absolute/external/icu-build/prefix-x86
```

The ICU build first creates native host tools, then cross-builds static target
common/i18n/data libraries and installs into its external prefix. Tests,
samples, extras, ICU I/O wrappers and target command-line tools are excluded;
Unicode/locale data and internationalization functionality are retained. Target
data generation uses the host tools through ICU's `--with-cross-build` contract.
JSC receives explicit target archive paths and `U_STATIC_IMPLEMENTATION`, never
host Linux libraries discovered by accident.

## Evidence and remaining ME work

The initial Windows ME API declaration probe fails compiling `putil.cpp` at
`GetLocaleInfoEx` / `LOCALE_NAME_USER_DEFAULT` / `LOCALE_SNAME`. Upstream's
`source/config/mh-mingw` itself defines `WINVER` / `_WIN32_WINNT` as `0x0601`.
A successful build with these upstream settings cannot establish Win9x support.

The host ICU build and `icu-smoke.cpp` executed successfully on Linux:
Korean NFC composition, `ko_KR` collation creation, timezone lookup and ICU
version 78.3 were checked. This is host evidence, not Windows execution.
The x86 target build, static-library installation and wrapper rerun also passed.
A real static x86 `icu-smoke.exe` linked successfully. Its ME import audit failed
on four KERNEL32 exports: `GetDynamicTimeZoneInformation`, `GetLocaleInfoEx`,
`LCIDToLocaleName`, and `LocaleNameToLCID`. The Windows executable has not run in
a guest. The full Unicode data archive is about 32 MiB; it was retained.

ICU's default MinGW installation attempted to put the static data library in
`bin`, leaving a tiny stub in `lib`. The wrapper fixes this through the supported
make override `MINGW_MOVEDLLSTOBINDIR=NO` and byte-compares the installed data
archive with the built full archive. JSC configure now finds all three target
ICU components and completes. Its first remaining compiler failure is in the
engine's 32-bit allocator path, described in [README.md](README.md).

A Win9x implementation must preserve user locale, Korean codepage handling,
timezone/DST semantics, synchronization, and any actually used file APIs.
The compiled and linked target import table determines concrete missing APIs;
source greps alone can include inactive architecture branches. Validate the
result in the actual guest after those replacements are implemented.

## Linked-binary import audit

Use `icu-smoke.cpp` to link actual common, i18n and data archives plus the C++
runtime. `audit-pe-imports.py` checks all named imported DLLs/functions against
a user-supplied, checksum-pinned OS baseline:

```
python3 porting/audit-pe-imports.py \
    --binary /absolute/external/icu-build/icu-smoke.exe \
    --dll-dir /absolute/extracted-target-dlls \
    --baseline /absolute/external/baseline.json \
    --output /absolute/external/icu-build/import-audit.json
```

Baseline format: `{"dlls": {"kernel32.dll": "verified-sha256", ...}}`.
Unknown DLLs, missing exports and unverifiable ordinal imports fail. Baseline
hash mismatches fail before compatibility can be claimed. The tool does not
verify runtime-loaded functions, forwarded API behavior or Win9x export stubs;
actual guest tests remain required.
