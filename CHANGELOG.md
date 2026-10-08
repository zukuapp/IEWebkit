# Changelog

## 0.1.0-dev.20261008 — 2026-10-08

First public development prerelease, containing the complete standalone
repository source, pinned dependency manifests and all local porting patches.

- Publish separate development document host components for all 17 IE/Windows
  profiles and 55 architecture/security-mode combinations, with per-artifact
  build metadata and linked PE/VERSIONINFO verification.
- Add reproducible development packaging that checks source and artifact hashes,
  includes compiler runtime notices, and produces SHA-256 checksums.
- Expand CI to cover the bounded renderer and its sanitizers, navigation harness
  builds and development packaging alongside the full host matrix.
- Document release reproduction, existing guest evidence, remaining engine gates
  and third-party patch/runtime notices.

The source checkpoint includes earlier Win9x ICU/WTF/OpenSSL porting changes,
the IE DocObject lifecycle/navigation integration and the HTML/CSS subset lab.
Recorded Windows ME diagnostics cover the COM lifecycle, a fixed local IE5.5
subset display and offline OpenSSL certificate checks. Their evidence and exact
scope remain in the component documentation.

Every browser target remains unverified. The host packages contain neither a
complete WebKit/JavaScriptCore engine nor an installer. Remote website rendering,
Protected Mode/Enhanced Protected Mode integration, JavaScript, WASM and complete
browser compatibility remain separate acceptance gates.
