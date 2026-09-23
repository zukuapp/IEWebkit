# IEWebkit

A site-independent WebKit and JavaScriptCore engine being ported into Internet
Explorer's document host. Intended organization repository:
`https://github.com/zukuapp/IEWebkit`.

**Development status: no complete browser engine or certified IE variant is
available yet.** The native engine contract and loader have host tests and an
x86 cross-compile check. WebKit source preparation is reproducible; the ME engine
port still needs dependencies and Windows API adaptations. Those checks are not
proof that websites render in Internet Explorer.

## Product boundary

Internet Explorer remains the visible browser, with its actual document URL,
navigation, history and window. The installed DocObject/ActiveX integration
hosts the engine. A standalone browser window, local HTML launcher, address-bar
spoof, or a site-specific screen protocol does not satisfy this product.

The common engine owns DOM, JavaScript, layout, painting, input and certificate-
validated networking. It must work with general sites without requiring their
servers to embed an OBJECT element. ZUKU-specific API/UI code stays in
`zukuapp/zuku-platform`; this repository has no dependency on that checkout.

## Layout

- `include/`: versioned engine ABI.
- `host/`: engine validation/loader and IE document host integration.
- `porting/`: pinned upstream source, cross-build tooling and measured API gaps.
- `variants.json`: separate IE 5.5, 6, 7, 8, 9, 10 and 11 target profiles.
- `tools/variants.py`: strict target selection and release eligibility checks.
- `test/`: host-side checks; guest certification is a separate requirement.

```sh
python3 tools/variants.py validate
python3 tools/variants.py select --ie 5.5 --os winme --arch x86 --mode classic
python3 -m unittest discover -s test -p 'test_*.py'
python3 -m unittest discover -s porting/tests -p 'test_*.py'
make -C host test docobject lifecycle-test-binary
```

Target selection describes development configurations. Adding `--release`
requires a reviewed guest verification record; currently every target is
unverified and release selection correctly fails. No installer should silently
substitute another browser version, bitness or security mode.

See [porting evidence](porting/README.md) and
[compatibility policy](docs/compatibility.md). For a specific IE/OS host build,
use [the variant builder](docs/build-variants.md). The lifecycle harness has
run in a real Windows ME guest; its scope and evidence are in
[the host notes](host/README.md). Source archives, Windows media,
keys, credentials, machine logs and compiled outputs are excluded from Git.
