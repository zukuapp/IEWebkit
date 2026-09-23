# Internet Explorer Active Document host

This is an implementation checkpoint, not a supported browser release. It is a
site-neutral COM Active Document, separately built from the engine and from any
ZUKU API adapter. It does not contain WebKit, JavaScriptCore, a mock HTML renderer
or a replacement browser shell.

`make -C host test docobject` runs contract checks and cross-compiles an x86
Windows 4.x-targeted `iewebkit-host.dll`. The host loads only the installed
adjacent `iewebkit-engine.dll`. The provider must export `IEWebKitGetEngineV1`,
identify its pinned source and implement the complete required C ABI. Without a
provider, in-place activation returns a missing-module error.

Implemented COM interfaces: IOleObject, IOleDocument, IOleDocumentView,
IOleInPlaceObject, IOleInPlaceActiveObject, IPersistMoniker and IOleCommandTarget.
The browser's real URL moniker supplies navigation. The actual engine reports
committed URL/origin and owns DOM, CSS, JavaScript, TLS, cookies and subresources.
The host creates a child surface inside the supplied IE in-place site, routes
size/focus/stop/refresh, and forwards top-level navigation through IWebBrowser2.
It never edits the address-bar window or maps a local file to a remote origin.

Registration creates only the new COM class and
`application/x-iewebkit-document` MIME association. It does not overwrite
`text/html`, Trident's CLSID, or global HTTP/HTTPS handlers. A version adapter must
select this DocObject on actual top-level browser navigation. That adapter and
the engine are still required; this DLL alone does not replace Trident.

## Remaining integration gates

- Connect a real WebKit provider, not a placeholder surface.
- Process-local top-level MIME/protocol handoff without server opt-in; preserve
  request method/body, redirects, fragments, downloads and headers. The current
  moniker load calls GET; POST ingress requires a captured binding transaction
  before enabling general browser interception.
- IE5.5/6 host identity, keyboard/IME, actual guest registration/activation.
- Distinct IE6+ own-navigation/history bridge and per-version adapters through
  IE11. COM layout and protected-mode behavior need actual variant tests.
- Do not publish or automatically install until lifecycle, request replay,
  navigation, TLS/origin, back/forward, content and guest tests pass.

## Primary architecture references

The [Microsoft IPersistMoniker contract](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/ms775042(v=vs.85))
provides browser moniker loading. Historical Chromium
[ChromeActiveDocument](https://chromium.googlesource.com/chromium/src/+/a9f74a6b78ecfb8f868d19d99e43a5679bb95ad4/chrome_frame/chrome_active_document.h)
shows a real DocObject embedding another renderer in IE, with explicit navigation
and history integration. Its [registration](https://chromium.googlesource.com/chromium/src/+/23.0.1271.56/chrome_frame/chrome_active_document.rgs)
uses a dedicated MIME type. These are architectural references; no Chrome Frame
code is incorporated into this host.
