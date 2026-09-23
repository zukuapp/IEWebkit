# Opt-in IE DocObject subset lab

This is a **local file renderer diagnostic**, not remote website support,
WebKit, TLS, or a release build. Build with `make -C renderer lab`.

The lab reuses the production COM lifecycle with `IEWK_SUBSET_LAB` defined and
links a separate private adapter rather than `host/engine_loader.cpp`. Its class
is `{A0AB22ED-E7C5-4C73-A0C1-1FD5A7B95206}`, MIME type is
`application/x-iewebkit-subset-lab`, and extension is `.iwksubset`. Normal host
builds retain their original identity, URL policy, and full-engine gate. Neither
lab DLL exports `IEWebKitGetEngineV1`; the internal lab function table declares
only the HTML/CSS bits and would fail the normal required-capability check.

## Prepare a disposable ME IE5.5/6 guest

1. Keep the sealed baseline; use an isolated overlay with no NIC. Close all IE
   windows before installing or replacing a previous lab build.
2. Put these four build outputs together on a read-only ISO:
   `SUBSETUP.EXE`, `SUBHOST.DLL`, `SUBSET.DLL`, `DEMO.IWK`.
3. Run `D:\SUBSETUP.EXE` (substitute the guest CD drive). It copies the three
   payloads to `C:\IEWKSUB`, renames the fixture to `DEMO.IWKSUBSET`, registers
   the distinct lab class/file type, and launches **Internet Explorer itself**
   using `CLSID_InternetExplorer`/`IWebBrowser2`.
4. IE navigates to `file:///C:/IEWKSUB/DEMO.IWKSUBSET`. Keep that genuine file
   address visible. Do not put a remote address on the local fixture. A second
   launch can use IE's File/Open or its address bar with the same file URL.
5. Collect `C:\SUBSET.LOG`, a screenshot showing IE/address/content, and the
   actual guest/IE version. `install.IE_navigate=0` means a navigation request
   was accepted, **not** that activation or painting succeeded. Require
   `subset.child_created=1`, `subset.layout_ok=1`, nonzero `subset.bytes` and
   visible styled/Hangul content. Resize and keyboard-scroll the actual IE view.
6. Close IE and check `subset.destroyed=1`. Run `SUBSETUP.EXE /remove` from the
   ISO to unregister only the lab class/MIME/extension. The fixture files stay
   in the disposable overlay. Shut down cleanly and verify baseline integrity.

The fixture starts with the 12-byte signature `IEWKSUBSET/1\n`, followed by
UTF-8 HTML. Only the exact fixed file reference is accepted; the adapter always
opens that one path, verifies the signature and bounds content to 1 MiB. It does
not follow links, execute scripts, fetch remote bytes, or pass credentials.
`IPersistMoniker` and lab-only `IPersistFile` both bind to this real file. No BHO
or system HTTP/HTTPS protocol handler is registered by this installer.

The file extension/ProgID/CLSID and MIME mappings follow Microsoft's documented
[COM class associations](https://learn.microsoft.com/en-us/windows/win32/com/hkey-local-machine-software-classes)
and [MIME association model](https://learn.microsoft.com/en-us/windows/win32/msi/mime-table).
Whether IE5.5 selects this DocObject for the fixture remains an actual guest test;
cross-compilation and COM registration alone cannot prove browser activation.

At packaging, **guest_verified=false**. Any successful local guest test proves
only this subset's IE in-place display. The separately developed verified native
TLS transport and a complete engine remain necessary for remote ZUKU pages.
