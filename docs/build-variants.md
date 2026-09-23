# Build a development host for one exact target

`tools/build_host_variant.py` compiles and links the native document host for a
selected profile in `variants.json`. It uses the exact IE version, Windows
profile, **content-process** architecture and security mode. The result is a
host DLL and object files, not a WebKit engine, installer, or compatibility
certification. The mode identifies the intended adapter target; it does not
claim that Protected Mode or AppContainer integration has been implemented.

```sh
python3 tools/build_host_variant.py \
  --profile ie55-winme --arch x86 --mode classic \
  --version 0.1.0.0 --work-dir /absolute/external/host-builds

python3 tools/build_host_variant.py \
  --profile ie11-win7-sp1 --arch x64 --mode protected \
  --version 0.1.0.0 --work-dir /absolute/external/host-builds
```

The profile ID, architecture and mode must form an exact existing combination.
There is no fallback to another IE, OS, bitness or less restrictive mode. A
missing x64 compiler is an error, even if the x86 toolchain is available.
`--release` first runs the existing guest-certification gate and refuses an
unverified target before creating outputs. This component builder does not
package certified engine releases even after a target obtains guest evidence.

Required target tools are `gcc`, `g++`, `windres`, and `objdump`, with prefix
`i686-w64-mingw32-` for x86 and `x86_64-w64-mingw32-` for x64. Every compiler
invocation receives an argument list without shell evaluation. Compiles are
sequential, each with a 120-second limit; tool inspection has a 15-second limit.

## Compiler declarations

| Windows profile | `WINVER` / `_WIN32_WINNT` | `NTDDI_VERSION` | PE subsystem |
| --- | --- | --- | --- |
| ME | `0x0410` / `0x0400` | `0x04000000` | 4.0 |
| XP SP3 | `0x0501` / `0x0501` | `0x05010300` | 5.1 |
| XP x64 SP2 | `0x0502` / `0x0502` | `0x05020200` | 5.2 |
| Vista SP2 | `0x0600` / `0x0600` | `0x06000200` | 6.0 |
| Windows 7 SP1 | `0x0601` / `0x0601` | `0x06010100` | 6.1 |
| Windows 8 | `0x0602` / `0x0602` | `0x06020000` | 6.2 |
| Windows 8.1 | `0x0603` / `0x0603` | `0x06030000` | 6.3 |

ME additionally declares `_WIN32_WINDOWS=0x0490`, using the conservative common
Windows 4.10 surface and an NT4 header baseline. These declarations do not make
modern compiler CRT imports run on ME; inspect imports and test the real guest.

`_WIN32_IE` is `0x0550`, `0x0600`, `0x0700`, `0x0800`, `0x0900`, or `0x0A00`.
The SDK defines both IE10 and IE11's header target as `0x0A00`. The independent
`IEWK_TARGET_IE` macro distinguishes them as `100` and `110` (IE5.5 is `55`).
`IEWK_TARGET_SECURITY_MODE` is `0` for classic, `1` for protected and `2` for
enhanced-protected. `IEWK_HOST_DEVELOPMENT=1` explicitly marks this build stage.
Defining these targets does not provide the missing per-version adapter code.

## Output ownership and evidence

Each result lives at:

```text
WORK_DIRECTORY/PROFILE-ARCH-MODE-host-vVERSION/
  iewebkit-host.dll
  objects/
  variant.rc
  build.json
  build.log
  pe-architecture.txt
```

The work directory must resolve outside the repository, including through
symlinks. Existing outputs are never reused or overwritten. An exclusive lock
prevents two runs from building the same variant concurrently. A crashed build
can leave its lock: inspect the process and stage before manually removing that
specific stale lock. A new version or external work directory provides a fresh
output location.

The version contains four decimal components in the Windows VERSIONINFO range
`0..65535`. The compiled resource includes the exact profile/architecture/mode,
version and unverified development status, so IE versions with otherwise shared
source still receive distinct artifact metadata. Sources are copied into a
private external stage before compilation. The DLL is published to its final
directory only after compilation, linking and architecture inspection pass.
Failure diagnostics use a sibling `*.failed.log`; they are not complete builds.

`build.json` records compiler versions, argument lists, selected declarations,
source hashes and output hashes. `engine_included`, `guest_verified`, and
`release_eligible` remain false. No source-only test or successful DLL link
changes those values. See [compatibility policy](compatibility.md) and
[host integration gaps](../host/README.md) for the remaining requirements.

## Checks

```sh
python3 -m unittest discover -s test -p 'test_build_variant.py'
```

Tests exercise all 17 profiles, exact target and mode selection, numeric version
and resource injection rejection, external/symlink directory boundaries,
existing-output preservation, concurrent ownership, missing toolchains and the
unverified release gate. Real representative compile checks cover IE5.5/ME
x86 and IE11/Windows 7 x86/x64. Those are host build results only; no guest or
engine rendering result is implied.
