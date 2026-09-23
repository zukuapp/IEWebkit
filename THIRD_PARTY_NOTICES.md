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

The IE document integration uses documented Microsoft COM interfaces.
Chrome Frame sources were consulted as architectural references for DocObject
integration; no Chrome Frame implementation is bundled here. Any future copied
upstream code must retain its original notices and provenance.

Windows installation media, system DLLs, product keys and proprietary platform
assets are not repository content. Guest tests use separately supplied media.
