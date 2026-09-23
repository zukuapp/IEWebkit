---
name: iewebkit-development
description: Port and test the independent IEWebkit WebKit/JavaScriptCore engine inside Internet Explorer 5.5 through 11, including per-version builds, native document hosting and Windows ME portability.
---

# IEWebkit development

Use the independent IEWebkit checkout, not a copy of engine files in a consuming
application repository. Its intended organization is `zukuapp`. Read the local
AGENTS.md and select the task's exact IE/OS/content-process architecture/security
mode with `tools/variants.py`; target existence is not a compatibility pass.

## Pick the implementation seam

- Engine portability: read `porting/README.md` and the pinned source manifests.
  Fetch dependencies into an external work directory. Audit the **compiled
  target's imports**, including dependencies and CRT. Search results for APIs
  in unused architecture branches are not proof of a target incompatibility.
- Browser embedding: read `include/engine.h` and `host/README.md`. IE remains
  the actual DocObject host with authoritative URL monikers and history.
  Preserve POST and redirect semantics in the protocol handoff. A local launch
  page or a separate browser window cannot prove remote-site integration.
- Compatibility/release: read `docs/compatibility.md` and `variants.json`.
  Each target needs its own guest evidence and artifact digest. Do not downgrade
  security mode or silently choose another IE/OS variant.

## Measured workflow

1. Separate host contract, import audit, linked build and actual guest tests.
2. Pin source/version/hash before patching. Keep local patches and original
   third-party notices; final engine distribution has separate license duties.
3. Reproduce a small failing target compile or guest behavior before changing
   the portability layer. Modern WebKit x64 support does not imply WinME x86
   support; C_LOOP interpreter bring-up is not full WASM/graphics completion.
4. Use disjoint parallel ownership for the engine port, IE host and guest tools
   when parallel work is requested. Keep host build outputs outside Git.
5. Test actual remote navigation, TLS validation, DOM/JS/layout, Korean input,
   forms, history and teardown on the selected guest. Scope guest tooling to
   the private lab; never publish Windows media, product keys or credentials.
6. Record failures and current gates precisely. `--release` requires complete,
   digest-bound guest evidence; passing source tests alone cannot certify it.

General web pages execute within the engine's web-origin model. Do not replace
arbitrary sites with a product-specific screen API or expose browser-host native
privileges to page scripts. A ZUKU API adapter belongs to the ZUKU repository.

## Repository sharing

Before a new repository checkpoint, verify the selected file list, excluded
artifacts, source provenance and third-party notices. Keep remote URLs free of
embedded credentials. Authentication failure is not a completed publication:
report local commit and remote creation/push status separately. Do not search
unrelated files for alternate tokens or print Git credential output.
