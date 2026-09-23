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
`no-pinshared` because `GetModuleHandleExW` is absent from Windows ME. It does
not disable certificate verification, hostname matching or SNI.

```
./network/build-winme-tls-probe.sh /path/openssl-3.5.8.tar.gz /external/work
python3 porting/audit-pe-imports.py \
  --binary /external/work/tls-smoke.exe \
  --dll-dir /path/to/extracted/checksum-pinned/ME/SYSTEM \
  --baseline /path/to/ME-dll-hashes.json \
  --output /external/work/me-imports.json
```

The probe accepts a public DNS host, explicit PEM CA bundle and output log
path. Defaults are `www.zuzunza.com`, `C:\\ZUKUQA\\cacert.pem` and
`C:\\ZUKUQA\\TLS.LOG`. Its request has no cookies or credentials. It requires
TLS 1.2 or newer, SNI, a trusted chain, and a matching DNS name, then records
the HTTP status. A status such as 403 can still demonstrate validated TLS;
it does not prove the website rendered. CA bytes and hash belong in the guest
test evidence and later installer manifest; do not embed a stale system store
or silently suppress failures when the ME clock or CA bundle is wrong.

Passing the import audit means only that a linked PE names functions present
in the selected ME DLLs. A guest run with actual network access is needed to
establish DNS, TCP, crypto initialization, handshake and CA validation. Once
that succeeds, WebCore's curl network backend still needs a Windows ME port
and its own request, redirect, cookie, cache and credential boundary tests.

## Offline guest gate when ME has no working NIC

`tls-offline.exe` runs a TLS handshake through an in-memory BIO pair. The server
uses a **disposable test-only** certificate for `iewebkit.invalid`. The client
trusts only the matching test CA, then repeats with a wrong hostname and must
reject it with `X509_V_ERR_HOSTNAME_MISMATCH`. It does not use guest networking or
prove that a Cloudflare HTTPS page loads. Generate fixture keys outside Git:

```
./network/generate-offline-fixture.sh /external/fixture
```

Stage `ca.pem`, `server.pem` and `server.key` on the guest test CD as `CA.PEM`,
`SERVER.PEM` and `SERVER.KEY`. The key is a disposable test fixture, never a
production credential. Record its hash and exclude the key from screenshots
and public artifacts. Run `D:\\TLSOFF.EXE` without arguments; it writes
`C:\\ZUKUQA\\TLSOFF.LOG` and exits zero only when both tests pass. The current
build's binary and fixture hashes must be recorded before comparing guest
output. File dates and the VM clock can affect certificate validity; do not
turn off time checks to make a test pass.
