# Incremental HTML/CSS subset prototype

This is an original, small parser/layout implementation and a Win32 child view.
It is **not WebKit**, JavaScriptCore, a complete browser engine, or a release
candidate. It deliberately does not export `IEWebKitGetEngineV1`, so the normal
DocObject engine capability gate cannot mistake it for a complete provider.

## Reproduce

```sh
make -C renderer test sanitize win32 preview
```

`build/iewk-subset.dll` is a static-runtime x86 DLL with subsystem 4.0. Its native
exports are `IEWKSubsetCreate`, `IEWKSubsetAppend`, `IEWKSubsetReset`, and
`IEWKSubsetPrepareUnload`. Call the last after destroying all children, before
releasing the final DLL reference, to unregister its window class. The first
creates a `WS_CHILD` view in a caller-provided window on the caller's UI thread;
there is no desktop browser window, URL bar, network connection, or script bridge.
A DocObject embedder must provide IE's actual in-place parent, own DLL lifetime
until all children are destroyed, resize the child with `MoveWindow`, and perform
navigation through the browser's authoritative moniker/origin path. The optional
link callback returns inert UTF-8 references; the embedder must resolve them
against the actual document URL and enforce navigation policy. It is never given
native file access or called from page JavaScript.

`build/example.svg` is an inspectable paint-list preview of `example.html` using
an approximate host font metric. It is **not an IE/GDI screenshot**. The Win32
view measures and paints with real GDI font metrics. `subset.h` is the portable
API; `win32_view.h` describes trusted native embedding. The DLL exports use the
Win32 `WINAPI` calling convention; the optional callback uses the C default
calling convention.

## Implemented subset

- Streamed UTF-8 input, including a character/entity split between chunks. The
  accumulated bounded document is parsed and laid out again after each append;
  this is incremental display, not a persistent DOM or incremental layout tree.
- Nested blocks and inline text; headings, paragraphs, bold/emphasis, breaks,
  simple list markers, collapsed ASCII whitespace and UTF-8-safe long-word wrap.
- Numeric character references and `amp`, `lt`, `gt`, `quot`, `apos`, `nbsp`.
- Embedded styles and inline declarations. Selectors: tag, `.class`, `#id`, `*`,
  comma-separated selector lists. Specificity/order and inline override apply.
- Color/background color (hex and a few named colors), pixel font size,
  bold/italic, underline, uniform pixel padding/margin, block/inline/none display.
- Block backgrounds, inherited text styling, vertical scrolling, keyboard
  scrolling, mouse link hit metadata. Relative, fragment, HTTP and HTTPS links
  may be returned; other schemes and control characters are rejected.
- Script raw text is never executed or displayed. Styles, head/title and template
  contents are not rendered as normal visible text.

## Limits and next gates

No DOM, JavaScript, forms, images, external styles/fonts, CSS comments/complex
selectors/media queries, tables, flex/grid, absolute positioning, CSS units other
than pixels, full HTML5 error recovery, bidi/shaping or accessibility tree exists.
Inline backgrounds/spacing and mixed-font baseline alignment are not implemented.
Margins add rather than collapse; overflowing glyphs can exceed a narrow viewport.
The Win32 view converts UTF-8 through the guest ANSI code page: Korean CP949 text
is the current intended lab case, not complete Unicode rendering. Link keyboard
focus traversal and history are still the embedder's work.

Limits are 1 MiB input, 4096 nodes, 64 parser stack entries, 256 CSS rules, 65536
paint commands, a 32768-pixel viewport and a 1000000-pixel document height. Font
sizes are bounded to 6–96 pixels and spacing to 256 pixels. Parse/layout failure
keeps the previous valid paint list and reports an error. `append` accepts bytes;
`layout` may subsequently reject their structure. Call `reset` before reusing a
completed document. Repeated small appends reparse the full buffer, so callers
should batch input; this is not a high-throughput streaming engine.

Host tests cover cascade, entities, byte-split streaming, nested layout, wrapping,
link filtering, raw-script closing delimiters, malformed input, input/node/depth/
rule limits, failed-measure preservation and reset. ASan/UBSan cover the same cases
plus a deterministic malformed-byte corpus. The cross-linked DLL imports were
compared with checksum-pinned actual Windows ME OEM DLL exports; this static gate
does not establish that each exported API works at runtime.

At the initial prototype checkpoint, **guest_verified=false** and
**full_engine_included=false**. A local DocObject lab test, an eventual verified
remote-byte integration, and complete WebKit/JS engine execution are separate
acceptance gates. No remote-site or IE-version compatibility pass follows from
these host tests or the SVG preview.
