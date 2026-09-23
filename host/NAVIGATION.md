# IE5.5 remote navigation development checkpoint

The host prototype now includes a separate Browser Helper Object and a
process-local URLMon HTTPS handoff. They select the existing IEWebkit DocObject
for an eligible top-level navigation without a local HTML launch page, server
`OBJECT` tag, address-bar editing, or global HTTPS registry override.

This is not a supported renderer. A normal adapter checks for a complete
adjacent engine provider before registering the handoff. The explicitly named
`navigation-diagnostic` target bypasses only that adapter availability check,
so an isolated guest can show where IE enters the real DocObject. The DocObject
still refuses to create a view without the engine. Never install or distribute
these diagnostic DLLs as a working browser.

## Flow and boundaries

1. The BHO accepts sites only inside `iexplore.exe`, obtains the actual
   IWebBrowser2 and subscribes to DWebBrowserEvents2. It compares COM IUnknown
   identity to exclude frame events.
2. A top-level BeforeNavigate2 cancels an older pending ticket. An HTTPS target
   with empty POST data and custom headers arms one exact canonical URL on the
   owning STA, expiring after ten seconds. IE5.5's twice-indirected VARIANT
   PostData is decoded with a bounded walk; cyclic values fail closed.
3. The temporary HTTPS handler checks the real BINDINFO GET verb and empty
   body. It consumes the ticket before callbacks, then reports the DocObject
   CLSID through `BINDSTATUS_CLSIDCANINSTANTIATE`. It emits no page bytes and
   makes no certificate, origin-commit, cookie, or HTTPS-success assertion.
4. Every other binding delegates to the system HTTPS protocol by its explicit
   CLSID, avoiding namespace recursion. The adapter does not convert POST to GET.
5. IE owns the original remote moniker and its address bar. The actual engine
   must subsequently fetch and commit the document with its own verified TLS,
   web-origin model and cookies. There is no renderer or TLS provider in this
   navigation component.

The prototype permits one adapter owner per process. Multiple browser windows,
frame/request races with identical URLs, non-GET/header-preserving engine
ingress, redirect/history synchronization, downloads, certificate indicators,
and later IE security modes remain release gates. The process-local HTTPS
registration is not chained with other add-ons, and Microsoft discourages
overriding HTTP(S) because of performance costs. No automatic installation is
provided.

## Build and guest probes

`make -C host navigation-test-binary` builds two native guest executables:

- `NAVTEST.EXE` uses actual URLMon binding and the actual host class factory. It
  checks remote moniker preservation after an explicit IPersistMoniker.Load,
  arbitrary-origin selection, local/userinfo URL rejection and duplicate-owner
  rejection. URLMon selects the object; the caller must load its moniker.
- `NAVVALUE.EXE` checks the exact BHO VARIANT decoder against nested values,
  cyclic references, nonempty authorization headers and nonempty POST data.

`make -C host navigation-diagnostic` additionally builds `NAVBHO.DLL` and
`IEWKHOST.DLL`. In a disposable offline guest only, register both with
`regsvr32 /s`, start the installed **Internet Explorer** executable directly at
`https://www.zuzunza.com/`, and capture `C:\ZUKUQA\IENAV.LOG` plus the visible IE
window. Close IE and unregister both DLLs with `regsvr32 /u /s` afterward.
No HTTP bootstrap or public TLS change is needed to test class selection.

`make -C host navigation-adapter` builds the normal development adapter with the
engine-availability gate enabled. It is still a prototype, not an installer or
a certified release.

## Actual Windows ME / IE5.5 result

The guest is Windows ME 4.90.3000, IE5.50.4134.0100, standard VGA, no NIC,
with a private serial QA channel. Standard VGA is a COM diagnostic configuration;
the separate Cirrus results remain unchanged.

- Real URLMon BindToObject selected the host class for both
  `https://www.zuzunza.com/` and another remote origin. Explicit Load then
  GetCurMoniker retained each exact URL. Local/userinfo/duplicate-adapter
  rejection passed. Result log SHA-256:
  `4dd20fcc744fabb92ccff9519262a2c38af865c3e0eab570724c2cfa22885596`.
- In the visible IE browser, the address and title retained the requested
  `https://www.zuzunza.com/`. The BHO attached, observed its own top-level
  navigation, armed GET successfully, and IE entered `Document.show`.
- `Document.show` reported `Document.engine_missing=1`. IE requests in-place
  activation before Load in this path, so absence of the engine prevents the
  subsequent browser-driven Load and rendered document. The screenshot's blank
  client area is a failed view activation, not WebKit-rendered content.
- That IE activation trace SHA-256 is
  `12a2be612f26f1facff3390aa288aab4e350032322304c5c3ffdbbfcf3402cac`.
- The native VARIANT negative tests passed in this guest; log SHA-256
  `ffcd16426976bebdcc31cb2554fd72d3f60a3e5d7bad64e26648db66db3ad53e`.
  Unregistration commands were run for both diagnostic DLLs, Windows reached its
  safe-to-power-off screen, and QEMU was stopped. The clean serial baseline
  hash remained unchanged; the experiment is preserved in a separate overlay.

No web response, certificate validation, authenticated operation, or WebKit
rendering pass is claimed. The remaining blocker is a complete engine provider
and its real TLS/navigation integration, followed by the gates listed above.

## Primary API references

- [IInternetSession.RegisterNameSpace](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa767759(v=vs.85)):
  temporary registration affects only the current process; handlers are not chained.
- [BINDSTATUS](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/ms775133(v=vs.85)):
  CLSIDCANINSTANTIATE selects the class returned by BindToObject; available since IE5.
- [IMoniker.BindToObject](https://learn.microsoft.com/en-us/windows/win32/api/objidl/nf-objidl-imoniker-bindtoobject):
  the caller obtains the requested interface on the identified object.
