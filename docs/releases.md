# Public development releases

The first public prerelease is
[`v0.1.0-dev.20261008`](https://github.com/zukuapp/IEWebkit/releases/tag/v0.1.0-dev.20261008).
Its Windows component VERSIONINFO uses `0.1.0.0`; the Git tag identifies the
dated source checkpoint. All repository source is committed to `main` and
included in the source archive. External upstream sources are reproduced using
the checksum-pinned manifests and bootstrap scripts.

## Contents

- Complete IEWebkit source archive, including host, renderer, networking probes,
  dependency manifests, local patches, tests, documentation and license notices.
- Development host archive for all 17 IE/Windows profiles and 55 exact
  architecture/security-mode combinations. Each host DLL includes build metadata
  and independent linked PE/VERSIONINFO verification. Compiler runtime license
  notices accompany the archive.
- SHA-256 checksums and machine-readable package metadata binding each component
  to the source revision, source digests and its exact artifact bytes.

Host components are native document integration building blocks. They contain
no WebKit or JavaScriptCore and are not complete browser installers. They retain
`engine_included=false`, `guest_verified=false`, `release_eligible=false`.
Those fields describe browser certification, independently of publishing a
GitHub development prerelease. No `--release` target gate is bypassed.

## Reproduce and verify

Use a fresh standalone checkout, Python 3.12+, GCC/G++, Make and both x86/x64
MinGW-w64 toolchains. Keep all outputs in a new directory outside the checkout.

```sh
git clone https://github.com/zukuapp/IEWebkit.git
cd IEWebkit
git checkout v0.1.0-dev.20261008
python3 tools/variants.py validate
python3 tools/variants.py select --ie 5.5 --os winme --arch x86 --mode classic
python3 -m unittest discover -s test -p 'test_*.py'
python3 -m unittest discover -s porting/tests -p 'test_*.py'
make -C host test docobject lifecycle-test-binary navigation-test-binary navigation-adapter
make -C renderer test sanitize win32 preview
python3 tools/build_host_matrix.py --jobs 2 --version 0.1.0.0 \
  --work-dir /absolute/external/new-host-matrix
python3 tools/verify_host_pe.py \
  /absolute/external/new-host-matrix/ie55-winme-x86-classic-host-v0.1.0.0
python3 tools/package_dev_release.py \
  --matrix /absolute/external/new-host-matrix/matrix-all.json \
  --output-dir /absolute/external/new-release-assets \
  --tag v0.1.0-dev.20261008 --revision v0.1.0-dev.20261008 \
  --runtime-notice /usr/share/doc/gcc-mingw-w64-base/copyright \
  --runtime-notice /usr/share/doc/mingw-w64-common/copyright
```

The example notice paths are for the Ubuntu/Debian MinGW-w64 packages. On other
systems, repeat `--runtime-notice` for the actual GCC/libstdc++ runtime license
and exception texts plus MinGW-w64 CRT/thread notices from the producing
toolchain. The packaging command validates the matrix and requires a new output
directory before producing an archive. See `--help` for all arguments. The source
archive can be reproduced
directly from the committed tag:

```sh
git archive --format=tar.gz --prefix=IEWebkit-v0.1.0-dev.20261008/ \
  --output=/absolute/external/IEWebkit-v0.1.0-dev.20261008-source.tar.gz \
  v0.1.0-dev.20261008
```

After downloading release assets, run `sha256sum -c SHA256SUMS` from their
directory. The release package manifest lists the source revision, filenames
and component hashes. A successful hash check proves byte integrity; it does
not certify guest behavior.

## Evidence and remaining work

- [Host lifecycle and COM evidence](../host/README.md).
- [Local Windows ME / IE5.5 subset lab evidence](../renderer/lab/GUEST-R5.md).
- [Pinned ICU, WTF and JavaScriptCore porting progress](../porting/README.md).
- [Offline Windows ME TLS positive and negative checks](../network/README.md).
- [Required exact-target browser certification](compatibility.md).

The JavaScriptCore bring-up has not produced `jsc.exe` or a complete engine.
The fixed local HTML/CSS subset is an independent diagnostic. Full DOM/JS,
remote HTTP/HTTPS integration, forms/input/history, WASM/media/graphics and each
IE/OS/security-mode guest test still need their own artifact-bound evidence.
Windows media, installed system binaries, guest disks, fixture keys, credentials
and private lab logs are excluded from source and release assets.
