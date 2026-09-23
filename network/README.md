# Windows ME native HTTPS dependency gate

Internet Explorer 5.5/6 cannot be relied on for current Cloudflare TLS. The
IEWebkit engine must make its own HTTPS connections with certificate chain and
hostname validation, while Internet Explorer remains the visible document host.
This directory proves one dependency path only; it is **not** a WebCore
resource loader, a website renderer, or an authenticated client.

The source is OpenSSL 3.5.8 from the official OpenSSL release archive. Its
published SHA-256 is
`a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2`.
The build script requires that exact archive and keeps source, licenses, build
objects and binaries outside this repository. The static x86 MinGW profile uses
`no-pinshared` because `GetModuleHandleExW` is absent from Windows ME. It uses
`no-comp` to omit obsolete TLS compression and `no-capieng` to omit the unused
Windows CryptoAPI engine. These settings do not disable certificate
verification, hostname matching or SNI.

The pinned local OpenSSL patch switches its internal reader/writer lock to
`InitializeCriticalSection` on Win9x. The spin-count entry point exists in the
ME export table but returned failure in the actual guest, so static import
success alone missed this dependency. A second patch selects OpenSSL's real
legacy Win32 mutex and condition-variable backend. With `_WIN32_WINNT=0x0400`,
upstream selected `OPENSSL_THREADS_NONE` while its RCU code still required those
objects. Both patches are selected explicitly with `IEWK_WIN9X`; other Windows
builds retain upstream behavior. The ME entropy patch uses
`CryptAcquireContextA` instead of the wide entry point still linked in R16
after the R15 guest's not-implemented failure. The precise failing call in R15
was not traced. Modern Windows retains upstream's wide call. The probe still
requires OpenSSL's normal random-seeding success.

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
rounds distinct without changing the TLS checks.

The third and fourth arguments are optional as a pair. When provided, the
script audits all five linked PE files and fails on a missing import or DLL
hash mismatch. The pinned DLL set must include `crypt32.dll` while the optional
OpenSSL winstore provider is linked. Results are `*-imports.json` in the work
directory. The
two-argument form leaves import auditing pending. Every run needs a new work
directory; the script refuses an existing extracted source tree. It checks
the compiled `no-comp` setting and that the Win32 object, rather than the
no-threads object, defines mutex and condition-variable functions.
`SHA256SUMS` records the source archive, production patches and executables.
The internal stage trace patches (`ssl-init`, `libctx-diag`, `namemap-diag`,
`cv-diag`) are opt-in investigation tools and are not applied by the normal
build. Instrumented and normal binaries have different
hashes; the final exact bytes need their own guest result.

The probe accepts a public DNS host, explicit PEM CA bundle and output log
path. Defaults are `www.zuzunza.com`, `C:\ZUKUQA\cacert.pem` and
`C:\ZUKUQA\TLS.LOG`. Its request has no cookies or credentials. It requires
TLS 1.2 or newer, SNI, a trusted chain, and a matching DNS name, then records
the HTTP status. A status such as 403 can still demonstrate validated TLS;
it does not prove the website rendered. CA bytes and hash belong in the guest
test evidence and later installer manifest; do not embed a stale system store
or silently suppress failures when the ME clock or CA bundle is wrong.

Passing the import audit means only that a linked PE names functions present
in the selected ME DLLs. The exported spin-count API failed at runtime in ME,
so import compatibility is a separate gate. A guest run with actual network
access is needed to establish DNS, TCP, crypto initialization, handshake and
CA validation. Once that succeeds, WebCore's curl network backend still needs a Windows ME port
and its own request, redirect, cookie, cache and credential boundary tests.

## Offline guest gate when ME has no working NIC

`tls-offline.exe` runs a TLS handshake through an in-memory BIO pair. The server
uses a **disposable test-only** certificate for `iewebkit.invalid`. The client
trusts only the matching test CA, then repeats with a wrong hostname and an
unrelated CA; both must be rejected. It does not use guest networking or
prove that a Cloudflare HTTPS page loads. Generate fixture keys outside Git:

```
./network/generate-offline-fixture.sh /external/fixture
```

Stage these files at the root of a read-only guest test CD:

| Host file | Guest path |
| --- | --- |
| `tls-offline.exe` | `D:\TLSOFF.EXE` |
| `tls-runner.exe` | `D:\TLSRUN.EXE` |
| the same `tls-offline.exe` for the private QA helper | `D:\ZUKUDIAG.EXE` |
| pinned and audited private serial QA helper | `D:\ZUKUQA.EXE` |
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

The optional private serial helper runs only the fixed `D:\ZUKUDIAG.EXE`
path; its source and guest result belong to the compatibility VM lab. A
successful helper transaction or runner exit is not sufficient by itself:
read the fresh `TLSOFF<suffix>.LOG` and require `valid=1`,
`invalid_rejected=1` and `untrusted_rejected=1`.

For the constructor probe, stage `tls-libctx-diag.exe` as `D:\TLSOFF.EXE`
beside the same runner. Read `C:\ZUKUQA\TLSCTX.LOG` and `TLSRUN.LOG` after
execution. A zero runner exit only means that this specific constructor probe
succeeded; it does not mean the certificate handshake passed.

The 2026-09-23 R13 run produced conflicting evidence: `TLSRUN.LOG` recorded
child exit zero while the retrieved `TLSCTX.LOG` reported
`libctx_new_failed`; an older stage log was also present. The R13 run is not
accepted as a constructor pass. Future rounds must use unique log names and
bind the staged executable hash to the retrieved logs. Neither the TLS
offline test nor public Cloudflare HTTPS is accepted yet. The current ME lab
has no proven working NIC for the public probe.

The R14 unique-media guest run did establish `OSSL_LIB_CTX_new/free` with
matching `TLSCTX14.LOG`, constructor stage logs and runner exit zero. It did
not run a TLS handshake; the offline positive and negative certificate cases
remain a separate guest gate.

The R15 ME offline log failed at client context construction with Windows error
120 (`ERROR_CALL_NOT_IMPLEMENTED`). R17 links the ANSI entropy call and omits
the CAPI engine. Its exact binaries passed the pinned ME import audit and its
source passed host positive, wrong-host and untrusted-CA checks. R17 still
imports `CertOpenSystemStoreW` through the optional system-store provider; the
offline probe loads an explicit PEM CA file instead. The R17 ME handshake and
random-seeding behavior remain unverified until its unique ISO runs in the
guest.
