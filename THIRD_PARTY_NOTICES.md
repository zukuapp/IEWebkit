# Third-party source and notices

The root BSD-2-Clause license covers original IEWebkit code only. It does not
relicense WebKit, JavaScriptCore, ICU, compiler runtimes or other dependencies.

`porting/webkit-source.json` pins upstream WebKit source and file hashes.
`porting/bootstrap.py` downloads it into an external directory. Upstream source
and binaries are not vendored in this repository's initial checkpoint.
Preserve all source-file notices and upstream copyright/license files when
patching, redistributing or packaging dependencies. WebKit contains components
under different licenses, including BSD-style licenses and LGPL.

Before any engine binary distribution, collect the exact dependency licenses,
corresponding source and local patches, and fulfill each applicable license's
source/relinking requirements. Source preparation and ABI tests alone do not
produce a distributable engine.

## Pinned dependency patches

- WebKitGTK 2.54.0 / JavaScriptCore: the exact upstream commit, archive and
  supplemental file hashes are in `porting/webkit-source.json`; local patch
  before/after hashes are in `porting/webkit-patches.json`. Preserve the BSD-style
  and LGPL notices in the upstream files and license documents. The repository
  includes the local patches, not the external upstream checkout.
- ICU4C 78.3: `porting/icu-source.json` pins the archive and upstream commit;
  `porting/icu-patches.json` pins the Win9x changes. The upstream manifest
  identifies Unicode-3.0; preserve ICU's LICENSE and bundled third-party notices.
- OpenSSL 3.5.8: `network/build-winme-tls-probe.sh` pins the source archive
  SHA-256, and `network/patches/` contains the Win9x changes and separate
  diagnostic patches. OpenSSL's upstream license is Apache-2.0. Preserve the
  upstream LICENSE.txt and copyright/attribution notices when redistributing
  modified OpenSSL. Production and diagnostic patch selection is documented in
  `network/README.md`.

These upstream libraries and their binaries are not included in the development
host release. Download their pinned sources into a separate work directory when
reproducing the porting work; preserve the local patch files and upstream notices.

## Development host compiler runtimes

The development host DLLs are linked with MinGW-w64 and GCC's static libgcc and
libstdc++ runtimes. Their source licenses remain separate from IEWebkit's license.
GCC runtime files carry GPL terms with the GCC Runtime Library Exception 3.1;
MinGW-w64 runtime code carries its own notices, including Zope Public License
2.1 and notices for individually marked components. Use the exact compiler
distribution's license texts for the toolchain that produced each artifact.

`tools/package_dev_release.py` includes the repository LICENSE and this document
from the selected Git revision, together with the supplied compiler runtime
notices. It records their hashes in the package manifest. Package only the
development host and verification metadata; do not add unreviewed dependency
DLLs, installed Windows system files or guest fixtures.

The IE document integration uses documented Microsoft COM interfaces.
Chrome Frame sources were consulted as architectural references for DocObject
integration; no Chrome Frame implementation is bundled here. Any future copied
upstream code must retain its original notices and provenance.

Windows installation media, system DLLs, product keys and proprietary platform
assets are not repository content. Guest tests use separately supplied media.
