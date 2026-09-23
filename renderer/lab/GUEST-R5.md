# Windows ME / IE5.5 subset display, R5

This is evidence for the **local HTML/CSS subset lab**, not the WebKit engine,
remote ZUKU, HTTPS or JavaScript. On 2026-09-23, an actual Korean Windows ME
guest reported Windows 4.90 build 3000, IE 5.50.4134.0100, ACP/OEMCP 949 and a
640×480 desktop through the private COM1 QA helper.

The guest used a fresh qcow2 overlay of the sealed IE5.5 QA baseline
(`/srv/hv/compat/overlays/winme-ie55-serial-qa-20260923.qcow2`, SHA-256
`4c6d261a2fd5f0b9a90149abe2d3f7e874c53c5f4947f850e8780b495b9412af`).
QEMU 10.1.5 used `pc-i440fx-4.2`, KVM, Pentium III, 256 MiB, one CPU,
`-vga cirrus`, loopback VNC and **no NIC**. This records visible Cirrus output,
not 3D acceleration. The ISO `iewk-paint-r5.iso` has SHA-256
`d31f89692364dc2c2471e47c75ee0ab39e6d9f32830ee738cd642272b54f9d88`.
Its DLLs were checked against checksum-pinned ME exports before boot:

| ISO member | SHA-256 |
| --- | --- |
| `SUBHOST.DLL` | `9a2180cd6333dcb0b422e897656c4c63c5ea8c5040e6a771080daaf7d3915542` |
| `SUBSET.DLL` | `98db63c816d97847b459223dd7a3f3570b6a07aef6205e117c000973d7bc4f14` |
| `SUBSETUP.EXE` | `dee7542e04e5e907bd01385186a05c9359407950bf85b0ae70bd5cfba8f64967` |
| `DEMO.IWK` | `3d8ec9bee10f6650d6fb8bd60080f730662da4d0503c3081abbd40ae3aed2523` |

The guest opened the ISO as D:, ran `SUBSETUP.EXE`, then displayed
`C:\IEWKSUB\DEMO.IWKSUBSET` **inside Internet Explorer**. The title bar, IE
menus/address bar and HTML/CSS heading/card were visible in
`r5-ie-initial.png` (SHA-256
`e3d28c587cc6783919faf816705841ca6c6973e487d363c520c0cdfd8c401a35`).
Page Down and Page Up changed the visible content; `r5-ie-pgup.png` (SHA-256
`26f5155cd008e86a2d11f9d336ff6b1c09ad26b1c17171e0f7f8a8d9770155d6`)
shows Korean text and the link within the styled card. IE was then closed.

`C:\SUBSET.LOG` was copied inside the guest to the fixed QA log directory and
pulled over private COM1. The resulting `SUBSET-R5.LOG` has SHA-256
`c41003def90e8096149c3b92dcfe3890bb3403378b5c3194d342ead4e735162e`.
It records successful COM registration/IE creation, a nonzero 472×194 document
rect, `subset.child_created=1`, `subset.bytes=0x329`, `subset.layout_ok=1`,
118 paint commands, successful text painting with zero failures, and
`subset.destroyed=1` after IE closed. `Document.remote_https=0` confirms only
the local fixture was used. The VM reached the Windows safe-to-power-off screen;
`qemu-img check` reported no errors and the sealed baseline hash was unchanged.

The preceding R4 guest had a zero rectangle and no visible painting. R5's
`IOleDocumentSite::ActivateMe` handoff plus fallback to the actual parent
client/context rectangle fixed that observed local display failure. The next
separate gates are IE6, real remote byte delivery with authenticated TLS,
JavaScriptCore/WebKit integration, navigation/history, forms and game graphics.

Evidence files are retained under
`/srv/zuku/deploy-work/20260923-iewk-subset-renderer/`; the current lab ISO and
screenshots are `iewk-paint-r5.iso`, `r5-ie-initial.png`, `r5-ie-pgup.png`,
`SUBSET-R5.LOG`, and `r5-shutdown-safe.png`.
