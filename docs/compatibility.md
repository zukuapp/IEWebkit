# Compatibility targets and evidence

IEWebkit has seven IE adapter families: IE 5.5, 6, 7, 8, 9, 10 and 11. Their
packaging IDs additionally distinguish Windows generation and the bitness of
the **IE content process**, not just the OS. IE6 on ME and IE6 on XP are separate
targets. Shared engine source does not make their runtime APIs interchangeable.

`variants.json` is a development target matrix, not a support announcement or
an exhaustive inventory of every Windows SKU. Unknown OS combinations fail
selection explicitly. Additional SKUs require explicit profiles and evidence.

The first guest target is Windows ME / IE5.5 x86, then ME / IE6 SP1. Later IE
versions are tested on the Windows generations in the matrix; installing IE7–11
on ME is not an assumption. Windows 98 remains unverified separately from ME.

For Vista and later, classic and Protected Mode need separate evidence. IE10/11
Enhanced Protected Mode needs its own AppContainer/bitness-aware integration;
ordinary COM activation is insufficient. The selector does not fall back to a
weaker mode. Microsoft's [policy documentation](https://learn.microsoft.com/en-us/windows/client-management/mdm/policy-csp-internetexplorer)
describes EPM's additional filesystem/registry and process restrictions.

The standalone IE application is disabled on some current Windows editions.
Edge IE mode and immersive IE are not interchangeable with the desktop IE host
and are not currently in this target matrix. See Microsoft's
[IE11 deployment guidance](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-it-pro/internet-explorer-11/ie11-deploy-guide/enable-and-disable-add-ons-using-administrative-templates-and-group-policy).

## Required guest certification

Each exact tuple must pass an installed, real guest run with:

1. Remote HTTP/HTTPS address entered in IE and actual document URL agreement.
2. WebKit layout, DOM and JavaScript execution on more than one independent site.
3. Certificate and hostname validation, redirects and origin isolation.
4. Korean display/input, keyboard/mouse/focus, forms and file selection.
5. History/back/forward, refresh, new windows and reliable unload/restart.
6. Account/session isolation from the browser bootstrap and other origins.
7. Correct DLL imports and process architecture; tested security mode retained.
8. An artifact SHA-256 and reviewable evidence for every required capability.

JS interpreter bring-up disables JIT and WASM temporarily. This is a build
stage, not the final feature promise. Rendering, WASM, media and acceleration
must each be measured before they are advertised. No current variant is marked
verified. Host contract tests and source/API audits are tracked separately.

Windows variants draw on Microsoft's published IE7 platform listings:
[MS07-004](https://learn.microsoft.com/en-us/security-updates/securitybulletins/2007/ms07-004)
and [MS07-027](https://learn.microsoft.com/en-us/security-updates/securitybulletins/2007/ms07-027).
The active source port is pinned and documented under `porting/`; upstream's
[Windows port](https://docs.webkit.org/Ports/WindowsPort.html) does not supply a
ready-made ME or general x86 engine.
