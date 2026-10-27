<!-- BEGIN ZUKU OFFICIAL BRAND -->
<!-- markdownlint-disable MD033 MD041 -->
<p align="center">
  <a href="https://docs.zuzunza.com/">
    <picture>
      <source media="(prefers-color-scheme: dark)"
        srcset="docs/branding/zuku-logo-dark.png">
      <img src="docs/branding/zuku-logo-light.png"
        alt="ZUKU" width="320">
    </picture>
  </a>
</p>
<p align="center">ZUKU - 내가 불러 일으키는 새로운 창작.</p>
<!-- markdownlint-enable MD033 MD041 -->
<!-- END ZUKU OFFICIAL BRAND -->

# IEWebkit

![version](https://img.shields.io/badge/version-v27.26102701-informational)

IEWebkit is a site-independent WebKit and JavaScriptCore engine being ported into
Internet Explorer's document host. Official source repository:
`https://github.com/zukuapp/IEWebkit`.

Current version: `v27.26102701`. See the [changelog](CHANGELOG.md).

## Status

No complete browser engine or certified IE variant is available. The native
engine contract and loader have host tests and cross-compiled development hosts
for all 55 declared combinations. WebKit source preparation is reproducible. The
ME engine port still needs dependencies and Windows API adaptations. These checks
do not show that websites render in Internet Explorer.

The published [development prerelease](https://github.com/zukuapp/IEWebkit/releases/tag/v0.1.0-dev.20261008)
contains the complete repository source and separately labelled development host
components for all 17 profiles / 55 architecture and security-mode combinations.
These host DLLs do not include WebKit or JavaScriptCore. See
[release contents and reproduction](docs/releases.md).

## Product boundary

Internet Explorer remains the visible browser, with its actual document URL,
navigation, history and window. The installed DocObject/ActiveX integration
hosts the engine. A standalone browser window, local HTML launcher, address-bar
spoof, or a site-specific screen protocol does not satisfy this product.

The common engine owns DOM, JavaScript, layout, painting, input and certificate-
validated networking. It must work with general sites without requiring their
servers to embed an OBJECT element. ZUKU-specific API/UI code stays in
`zukuapp/zuku-platform`; this repository has no dependency on that checkout.

## Requirements

- Python 3.12+.
- GCC, G++ and Make.
- MinGW-w64 cross toolchains for x86 (`i686-w64-mingw32-`) and x64
  (`x86_64-w64-mingw32-`).

CI runs on `ubuntu-24.04` (`.github/workflows/host-checks.yml`). The engine port
has additional prerequisites; see [porting evidence](porting/README.md).

## Repository layout

- `include/`: versioned engine ABI.
- `host/`: engine validation/loader and IE document host integration.
- `porting/`: pinned upstream source, cross-build tooling and measured API gaps.
- `network/`: pinned, certificate-validating Windows ME HTTPS dependency probe.
- `renderer/`: bounded HTML/CSS subset and opt-in local IE DocObject diagnostic.
- `variants.json`: separate IE 5.5, 6, 7, 8, 9, 10 and 11 target profiles.
- `tools/variants.py`: strict target selection and release eligibility checks.
- `tools/package_dev_release.py`: digest-checked development host packaging.
- `test/`: host-side checks; guest certification is a separate requirement.

Source archives, Windows media, keys, credentials, machine logs and compiled
outputs are excluded from Git.

## Usage

Validate target selection and run the host checks:

```sh
python3 tools/variants.py validate
python3 tools/variants.py select --ie 5.5 --os winme --arch x86 --mode classic
python3 -m unittest discover -s test -p 'test_*.py'
python3 -m unittest discover -s porting/tests -p 'test_*.py'
make -C host test docobject lifecycle-test-binary
make -C renderer test sanitize win32 preview
```

Target selection describes development configurations. Adding `--release`
requires a reviewed guest verification record. Every target is unverified, so
release selection fails. An installer must not silently substitute another
browser version, bitness or security mode.

To build a host for a specific IE/OS target, use
[the variant builder](docs/build-variants.md). Target policy is in
[compatibility policy](docs/compatibility.md). The lifecycle harness has run in
a real Windows ME guest; its scope and evidence are in
[the host notes](host/README.md).

## Changelog

See [CHANGELOG.md](CHANGELOG.md).
