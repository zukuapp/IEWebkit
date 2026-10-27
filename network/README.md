# Windows ME native HTTPS dependency gate

Internet Explorer 5.5/6 cannot negotiate current TLS. The IEWebkit engine must
make its own HTTPS connections with certificate chain and hostname validation,
while Internet Explorer remains the visible document host. This directory builds
and tests the OpenSSL dependency for that path.

The source is OpenSSL 3.5.8 from the official OpenSSL release archive. Its
published SHA-256 is
`a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2`.
The build script requires that exact archive and keeps source, licenses, build
objects and binaries outside this repository. The static x86 MinGW profile uses
`no-pinshared` because `GetModuleHandleExW` is absent from Windows ME. It uses
`no-comp` to omit obsolete TLS compression and `no-capieng` to omit the unused
Windows CryptoAPI engine. Certificate verification, hostname matching and SNI
remain enabled.

The pinned local OpenSSL patch switches its internal reader/writer lock to
`InitializeCriticalSection` on Win9x. The spin-count entry point exists in the
ME export table but returned failure in the actual guest, so static import
success alone missed this dependency. A second patch selects OpenSSL's real
legacy Win32 mutex and condition-variable backend. With `_WIN32_WINNT=0x0400`,
upstream selected `OPENSSL_THREADS_NONE` while its RCU code still required those
objects. Both patches are selected explicitly with `IEWK_WIN9X`; other Windows
builds retain upstream behavior. The ME entropy patch uses
`CryptAcquireContextA` instead of the wide entry point, which returned
not-implemented on ME. ME's CryptoAPI providers also reject `CRYPT_SILENT` with
`NTE_BAD_FLAGS`; the Win9x patch retries only that error without the
unsupported flag. It keeps `CRYPT_VERIFYCONTEXT` and credits entropy only
after `CryptGenRandom` succeeds. Modern Windows retains upstream's wide call
and flags. [Microsoft's CryptoAPI reference](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-cryptacquirecontexta)
defines `NTE_BAD_FLAGS` and recommends `CRYPT_VERIFYCONTEXT` when persisted keys
are unnecessary.

```
./network/build-winme-tls-probe.sh \
  /path/openssl-3.5.8.tar.gz /external/new-work \
  /path/to/extracted/checksum-pinned/ME/SYSTEM \
  /path/to/ME-dll-hashes.json
```

For a new sealed guest round, set `IEWK_QA_LOG_SUFFIX` to one or two digits
before the build, for example `IEWK_QA_LOG_SUFFIX=15`. The offline probe then
writes `TLSOFF15.LOG`; its fixed runner writes `TLSRUN15.LOG` and
`TLSOUT15.LOG`. The work directory records the suffix and source hashes in
`qa-log-suffix.txt` and `SHA256SUMS`. This keeps logs from different media
rounds distinct without changing the TLS checks. Bind the staged executable hash
to the retrieved logs.

The third and fourth arguments are optional as a pair. When provided, the
script audits all five linked PE files and fails on a missing import or DLL
hash mismatch. The pinned DLL set must include `crypt32.dll` while the optional
OpenSSL winstore provider is linked. Results are `*-imports.json` in the work
directory. The two-argument form leaves import auditing pending. Every run
needs a new work directory; the script refuses an existing extracted source
tree. It checks the compiled `no-comp` setting and that the Win32 object, rather
than the no-threads object, defines mutex and condition-variable functions.
`SHA256SUMS` records the source archive, production patches and executables.
The stage trace patches (`ssl-init`, `libctx-diag`, `namemap-diag`, `cv-diag`)
are opt-in investigation tools; the normal build does not apply them.
Instrumented and normal binaries have different hashes; the final exact bytes
need their own guest result.

The probe accepts a public DNS host, explicit PEM CA bundle and output log path.
Defaults are `www.zuzunza.com`, `C:\ZUKUQA\cacert.pem` and `C:\ZUKUQA\TLS.LOG`.
Its request has no cookies or credentials. It requires TLS 1.2 or newer, SNI, a
trusted chain, and a matching DNS name, then records the HTTP status. A status
such as 403 can still demonstrate validated TLS. CA bytes and hash belong in the
guest test evidence and later installer manifest; do not embed a stale system
store or silently suppress failures when the ME clock or CA bundle is wrong.

A guest run with actual network access is needed to establish DNS, TCP, crypto
initialization, handshake and CA validation.

## Offline guest gate when ME has no working NIC

`tls-offline.exe` runs a TLS handshake through an in-memory BIO pair. The server
uses a disposable test-only certificate for `iewebkit.invalid`. The client
trusts only the matching test CA, then repeats with a wrong hostname and an
unrelated CA; both must be rejected. Generate fixture keys outside Git:

```
./network/generate-offline-fixture.sh /external/fixture
```

Stage these files at the root of a read-only guest test CD:

| Host file | Guest path |
| --- | --- |
| `tls-offline.exe` | `D:\TLSOFF.EXE` |
| `tls-runner.exe` | `D:\TLSRUN.EXE` |
| the same `tls-offline.exe` for the QA helper | `D:\ZUKUDIAG.EXE` |
| pinned and audited serial QA helper | `D:\ZUKUQA.EXE` |
| fixture `ca.pem` | `D:\CA.PEM` |
| fixture `otherca.pem` | `D:\OTHERCA.PEM` |
| fixture `server.pem` | `D:\SERVER.PEM` |
| fixture `server.key` | `D:\SERVER.KEY` |

The key is a disposable test fixture, never a production credential. Record
each staged file's SHA-256 and exclude the key from screenshots and public
artifacts. Run `D:\TLSRUN.EXE` without arguments. The fixed 60-second runner
records the child exit code in `C:\ZUKUQA\TLSRUN.LOG`; the child writes
`C:\ZUKUQA\TLSOFF.LOG` and exits zero only when all three tests pass. File
dates and the VM clock can affect certificate validity; do not turn off time
checks to make a test pass.

The packaging script verifies byte-for-byte ISO extraction and requires PE
import audit reports that match the exact offline, runner and QA helper bytes:

```
./network/package-winme-tls-offline-iso.sh \
  /external/build /external/fixture /external/ZUKUQA.EXE \
  /external/qa-helper-imports.json /external/offline-qa.iso
```

The optional serial helper runs only the fixed `D:\ZUKUDIAG.EXE` path. Judge
the run from the fresh `TLSOFF<suffix>.LOG`, not from the helper transaction or
runner exit, and require `valid=1`, `invalid_rejected=1` and
`untrusted_rejected=1`.

For the constructor probe, stage `tls-libctx-diag.exe` as `D:\TLSOFF.EXE`
beside the same runner. Read `C:\ZUKUQA\TLSCTX.LOG` and `TLSRUN.LOG` after
execution. A zero runner exit means only that this constructor probe
succeeded.

The CryptoAPI diagnostic `tls_csp_diag.c` enumerates installed providers and
records only the success of `CryptGenRandom`, not the generated bytes. On ME,
every provider rejected `CRYPT_VERIFYCONTEXT | CRYPT_SILENT` with
`NTE_BAD_FLAGS` (`0x80090009`), and the default provider accepted
`CRYPT_VERIFYCONTEXT` alone. For another CSP survey, compile that source with
the same WinME target defines used by `build-winme-tls-probe.sh`, link
`-ladvapi32`, run the pinned PE import audit, and stage it as `D:\\ZUKUDIAG.EXE`
with the serial QA helper.

The clean build includes the narrow `NTE_BAD_FLAGS` retry and passed the real
Windows ME offline gate. Its first guest run rejected the valid fixture with
X.509 verify code 9 (certificate not yet valid), while still rejecting the wrong
hostname (62) and unrelated CA (20). The guest's Date/Time UI showed the Seoul
GMT+09 timezone but displayed the host's UTC hour as its local hour; the guest's
effective UTC was nine hours behind. In the disposable overlay, setting local
date/time to the actual Seoul time, 2026-09-24 02:50, allowed the unmodified
2026-09-23 15:54 UTC test certificate to validate. The rerun logged TLS 1.3 with
verify 0 for the valid hostname, 62 for the wrong hostname and 20 for the
unrelated CA, with `RESULT valid=1 invalid_rejected=1 untrusted_rejected=1`. The
fixed guest runner also recorded child exit zero. The exact read-only ISO
SHA-256 is `324766ec07784131e46775fc4d218265099ae408345955a6ecf3e1f34f495a0a`;
the corrected-clock guest log SHA-256 is
`d7cff844944078d4a33e03965e119a3bfbc3d579f157bf6b9f8a2e7e45c9674f`. This proves
the OpenSSL dependency in an offline ME process.
