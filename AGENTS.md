# IEWebkit engineering

This is a standalone, site-independent browser-engine repository. Keep ZUKU
product adapters and deployment secrets in their owning repositories.

Use disjoint agent ownership for host, engine port and target verification.
Never overwrite another agent's changes or commit a whole external checkout.
Pin upstream source versions and hashes; preserve upstream license notices.
Keep dependency sources, build outputs and guest images outside this repository.

All IE 5.5 through IE 11 variants require separate target selection, packaging
and actual guest verification. A C ABI test, import audit or successful build is
not a rendering test. Preserve Internet Explorer as the real document host and
test remote URL navigation, history, TLS, DOM/JS, input and unload behavior.
Do not weaken certificate checks or silently disable Protected Mode to pass a
test. Record failures and missing capabilities accurately.
